/* gtklistview.c
 * Copyright (C) 2012 Benjamin Otte <otte@gnome.org>
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

#include "gtklistview.h"

#include "gtkadjustment.h"
#include "gtkenums.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkscrollbar.h"
#include "gtktypebuiltins.h"

#include <math.h>

/**
 * SECTION:gtklistview
 * @title: GtkListView
 * @short_description: A widget which displays a list of items
 *
 * #GtkListView provides a view on a #GtkTreeModel that is not based on cell
 * renderers. It displays the model as a list of regular #GtkWidgets instead.
 *
 * Like #GtkTreeView or #GtkIconView, it allows to select one or multiple items
 * (depending on the selection mode, see gtk_list_view_set_selection_mode()).
 * In addition to selection with the arrow keys, #GtkListView supports
 * rubberband selection, which is controlled by dragging the pointer.
 *
 * Note that if the tree model is backed by an actual tree store (as
 * opposed to a flat list where the mapping to lists is obvious),
 * #GtkListView will only display the first level of the tree and
 * ignore the tree's branches.
 *
 * Since: 3.6
 */

typedef struct _GtkListViewItem GtkListViewItem;

typedef GtkWidget * (* GtkListViewCreateFunc) (gpointer data);
typedef void        (* GtkListViewUpdateFunc) (GtkWidget *widget, GtkTreeModel *model, GtkTreeIter *iter, gpointer data);

struct _GtkListViewItem {
  guint                 pos;
  GtkWidget *           widget;
  GtkListViewItem *     next;
};

struct _GtkListViewPrivate {
  GtkTreeModel *        model;
  GtkSelectionMode      selection_mode;
  GtkListViewItem *     items;

  GtkListViewCreateFunc widget_create;
  GtkListViewUpdateFunc widget_update;
  GDestroyNotify        widget_data_destroy;
  gpointer              widget_data;

  GtkWidget *           scrollbar;

  GtkListViewItem *     top;                    /* first displayed item or NULL if none */
  GtkListViewItem *     start;                  /* first 'instantiated' item relevant for size requests or NULL if none */
  GtkListViewItem *     end;                    /* last 'instantiated' item relevant for size requests or NULL if none */
  gint                  default_height;         /* height used for 'noninstantiated' items, set in size_allocate */
};

/* Signals */
enum
{
  ITEM_ACTIVATED,
  SELECTION_CHANGED,
  SELECT_ALL,
  UNSELECT_ALL,
  SELECT_CURSOR_ITEM,
  TOGGLE_CURSOR_ITEM,
  MOVE_CURSOR,
  ACTIVATE_CURSOR_ITEM,
  LAST_SIGNAL
};

/* Properties */
enum
{
  PROP_0,
  PROP_MODEL,
  PROP_SELECTION_MODE,
  PROP_BUILDER_XML
};

G_DEFINE_TYPE (GtkListView, gtk_list_view, GTK_TYPE_CONTAINER)

static void
gtk_list_view_item_free (GtkListViewItem *item)
{
  if (item->next)
    gtk_list_view_item_free (item->next);

  gtk_widget_unparent (item->widget);
  g_slice_free (GtkListViewItem, item);
}

static gboolean
gtk_list_view_contains_path (GtkListView *list_view,
                             GtkTreePath *path)
{
  return gtk_tree_path_get_depth (path) == 1;
}

static void
gtk_list_view_clear (GtkListView *list_view)
{
  GtkListViewPrivate *priv = list_view->priv;

  if (priv->items)
    {
      gtk_list_view_item_free (priv->items);
      priv->items = NULL;
    }

  priv->top = NULL;
  priv->start = NULL;
  priv->end = NULL;
}

static guint
gtk_list_view_get_n_items (GtkListView *list_view)
{
  GtkListViewPrivate *priv = list_view->priv;

  if (priv->model == NULL)
    return 0;

  return gtk_tree_model_iter_n_children (priv->model, NULL);
}

/* I don't want to make this function public, it exposes too many internals.
 * Also, experience from GtkCellLayoutDataFunc says that people are too
 * careless about ensuring the provided functions are fast and idempotent.
 * So I'd rather provide a bunch of wrappers.
 */
static void
gtk_list_view_set_widget_creation_funcs (GtkListView           *list_view,
                                         GtkListViewCreateFunc  create_func,
                                         GtkListViewUpdateFunc  update_func,
                                         GDestroyNotify         destroy_func,
                                         gpointer               data)
{
  GtkListViewPrivate *priv;

  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (create_func != NULL);

  priv = list_view->priv;

  if (priv->widget_data_destroy)
    priv->widget_data_destroy (priv->widget_data);

  priv->widget_create = create_func;
  priv->widget_update = update_func;
  priv->widget_data_destroy = destroy_func;
  priv->widget_data = data;

  gtk_list_view_clear (list_view);
  gtk_widget_queue_resize (GTK_WIDGET (list_view));
}

static GtkWidget *
gtk_list_view_default_widget_create (gpointer unused)
{
  GtkWidget *widget;
  
  widget = gtk_label_new ("");
  gtk_widget_show (widget);

  return widget;
}

static void
gtk_list_view_default_widget_update (GtkWidget    *widget,
                                     GtkTreeModel *model,
                                     GtkTreeIter  *iter,
                                     gpointer      unused)
{
  GString *string;
  guint i;
  
  string = g_string_new (NULL);

  for (i = 0; i < gtk_tree_model_get_n_columns (model); i++)
    {
      GValue value = G_VALUE_INIT;
      gchar *s;

      if (i != 0)
        g_string_append (string, ", ");
      gtk_tree_model_get_value (model, iter, i, &value);
      s = g_strdup_value_contents (&value);
      g_value_unset (&value);
      g_string_append (string, s);
      g_free (s);
    }
  gtk_label_set_text (GTK_LABEL (widget), string->str);
  g_string_free (string, TRUE);
}

static GtkWidget *
gtk_list_view_create_widget (GtkListView *list_view,
                             guint        index)
{
  GtkListViewPrivate *priv = list_view->priv;
  GtkTreeIter iter;
  GtkWidget *widget;

  widget = priv->widget_create (priv->widget_data);

  if (!gtk_tree_model_iter_nth_child (priv->model, &iter, NULL, index))
    {
      g_assert_not_reached ();
    }

  priv->widget_update (widget,
                       priv->model,
                       &iter,
                       priv->widget_data);

  return widget;
}

static GtkListViewItem *
gtk_list_view_create_item (GtkListView *list_view,
                           guint        index)
{
  GtkListViewItem *item;

  item = g_slice_new0 (GtkListViewItem);

  item->pos = index;
  item->widget = gtk_list_view_create_widget (list_view, index);
  gtk_widget_set_parent (item->widget, GTK_WIDGET (list_view));

  return item;
}

static GtkListViewItem *
gtk_list_view_get_item (GtkListView *list_view,
                        guint        index,
                        gboolean     create)
{
  GtkListViewPrivate *priv = list_view->priv;
  GtkListViewItem *item, *prev;

  prev = NULL;
  for (item = priv->items;
       item != NULL && item->pos < index;
       item = item->next)
    {
      prev = item;
    }

  if (item && item->pos == index)
    return item;

  if (create)
    {
      GtkListViewItem *new_item = gtk_list_view_create_item (list_view, index);

      if (prev)
        prev->next = new_item;
      else
        priv->items = new_item;
      new_item->next = item;

      return new_item;
    }
  else
    return NULL;
}

/* We treat the list of rows in the treemodel like a scrollbar:
 * +-----------+---------+-------+
 * |           |xxxxxxxxx|       |
 * +-----------+---------+-------+
 * The slider is the area we care about for size requests and is reduced
 * in size. Only for these items do we instantiate widgets. For everything
 * else, we just guess width/height and everything else. This function
 * ensures this slider exists, and that the visible area is inside this
 * slider.
 * The area goes from
 *   priv->first
 * to
 *   priv->last (inclusive)
 *
 * Note that a bunch of items might still exist in priv->items that are not
 * part of this slider. Those are special rows (like the focus when not
 * on screen). They are never involved in size computations though.
 */
static void
gtk_list_view_update_items (GtkListView *list_view)
{
  GtkListViewPrivate *priv = list_view->priv;
  GtkListViewItem *item, *prev;
  guint i, n_cached_items, n_total_items, first, last;

  /* number of items that we intend to cache. This
   * is mostly random, there is only one rather important requirement:
   * We must have enough children instantiated for the size requests to
   * work, as adding or removing items would mess up the sizes.
   * And we have no idea in here what the size requests expect from us,
   * so we can't do anything but do a large enough guess.
   * Note that we can always increase that number, it'll just cause
   * performance and memory increases (that hopefully should be linear).
   */
  n_total_items = gtk_list_view_get_n_items (list_view);
  n_cached_items = gdk_screen_get_height (gtk_widget_get_screen (GTK_WIDGET (list_view)));
  n_cached_items = MIN (n_cached_items, n_total_items);
  if (priv->top)
    first = priv->top->pos;
  else
    first = 0;
  first = MIN (first, n_total_items - n_cached_items);
  last = first + n_cached_items;

  item = priv->items;
  prev = NULL;
  i = first;
  while (item)
    {
      if (item->pos < i || item->pos > last)
        {
          GtkListViewItem *free_me = item;
          if (prev)
            prev->next = item->next;
          else
            priv->items = item->next;

          item = item->next;
          free_me->next = NULL;
          gtk_list_view_item_free (free_me);
          continue;
        }

      for (; i < last && i < item->pos; i++)
        {
          GtkListViewItem *new_item = gtk_list_view_create_item (list_view, i);

          if (prev)
            prev->next = new_item;
          else
            priv->items = new_item;
          new_item->next = item;
          prev = new_item;
        }
      if (i == item->pos)
        i++;

      prev = item;
      item = item->next;
    }
  for (; i < last; i++)
    {
      GtkListViewItem *new_item = gtk_list_view_create_item (list_view, i);

      if (prev)
        prev->next = new_item;
      else
        priv->items = new_item;
      new_item->next = item;
      prev = new_item;
    }

  if (priv->top == NULL)
    priv->top = gtk_list_view_get_item (list_view, first, FALSE);
  priv->start = priv->top;
  priv->end = gtk_list_view_get_item (list_view, last - 1, FALSE);
  g_assert (priv->start == NULL || priv->end != NULL);
}

static GtkSizeRequestMode
gtk_list_view_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

/* FIXME: Also allow using natural width for scrollbar */
static gint
gtk_list_view_get_scrollbar_width (GtkListView *list_view)
{
  GtkListViewPrivate *priv = list_view->priv;
  gint size;

  gtk_widget_get_preferred_width (priv->scrollbar, &size, NULL);
  
  return size;
}

static void
gtk_list_view_get_preferred_width (GtkWidget *widget,
				   gint      *minimum,
				   gint      *natural)
{
  GtkListView *list_view = GTK_LIST_VIEW (widget);
  GtkListViewPrivate *priv = list_view->priv;
  GtkListViewItem *item;
  gint min_total, nat_total;
  gint min_child, nat_child;

  gtk_list_view_update_items (list_view);

  min_total = 0;
  nat_total = 0;
  if (priv->start != NULL)
    {
      for (item = priv->start; item != priv->end->next; item = item->next)
        {
          gtk_widget_get_preferred_width (item->widget, &min_child, &nat_child);
          min_total = MAX (min_total, min_child);
          nat_total = MAX (nat_total, nat_child);
        }
    }

  min_child = gtk_list_view_get_scrollbar_width (list_view);
  min_total += min_child;
  nat_total += min_child;

  *minimum = min_total;
  *natural = nat_total;
}

static void
gtk_list_view_get_preferred_width_for_height (GtkWidget *widget,
                                              gint       height,
                                              gint      *minimum,
                                              gint      *natural)
{
  /* FIXME */
  gtk_list_view_get_preferred_width (widget, minimum, natural);
}

static void
gtk_list_view_get_heights (GtkListViewItem *item,
                           GtkListViewItem *end,
                           gint             for_size,
                           gint            *min_max,
                           gint            *min_sum,
                           guint           *n_items)
{
  int child_height;

  if (min_max)
    *min_max = 0;
  if (min_sum)
    *min_sum = 0;
  if (n_items)
    *n_items = 0;

  for (; item != end; item = item->next)
    {
      /* We only ever allocate min sizes, so ignore the other one */
      if (for_size < 0)
        gtk_widget_get_preferred_height (item->widget, &child_height, NULL);
      else
        gtk_widget_get_preferred_height_for_width (item->widget, for_size, &child_height, NULL);
      if (min_max)
        *min_max = MAX (*min_max, child_height);
      if (min_sum)
        *min_sum += child_height;
      if (n_items)
        *n_items += 1;
    }
}

static void
gtk_list_view_get_preferred_height (GtkWidget *widget,
				    gint      *minimum,
				    gint      *natural)
{
  GtkListView *list_view = GTK_LIST_VIEW (widget);
  GtkListViewPrivate *priv = list_view->priv;
  guint n_items, n_measured_items;
  gint min_total, nat_total;
  gint min_child, nat_child;

  gtk_list_view_update_items (list_view);

  gtk_list_view_get_heights (priv->start, priv->end, -1, &min_total, &nat_total, &n_measured_items);
  if (n_measured_items)
    {
      n_items = gtk_list_view_get_n_items (list_view);

      /* XXX: overflow? */
      nat_child = nat_total / n_measured_items;
      nat_total += nat_child * (n_items - n_measured_items);
    }

  gtk_widget_get_preferred_height_for_width (priv->scrollbar,
                                             gtk_list_view_get_scrollbar_width (list_view),
                                             &min_child, &nat_child);
  min_total = MAX (min_total, min_child);
  nat_total = MAX (nat_total, nat_child);

  *minimum = min_total;
  *natural = nat_total;
}

static void
gtk_list_view_get_preferred_height_for_width (GtkWidget *widget,
                                              gint       width,
                                              gint      *minimum,
                                              gint      *natural)
{
  GtkListView *list_view = GTK_LIST_VIEW (widget);
  GtkListViewPrivate *priv = list_view->priv;
  guint n_measured_items, n_items;
  gint scrollbar_width;
  gint min_total, nat_total;
  gint min_child, nat_child;

  gtk_list_view_update_items (list_view);

  scrollbar_width = gtk_list_view_get_scrollbar_width (list_view);
  width -= scrollbar_width;

  gtk_list_view_get_heights (priv->start, priv->end, width, &min_total, &nat_total, &n_measured_items);
  if (n_measured_items)
    {
      n_items = gtk_list_view_get_n_items (list_view);

      /* XXX: overflow? */
      nat_child = nat_total / n_measured_items;
      nat_total += nat_child * (n_items - n_measured_items);
    }

  gtk_widget_get_preferred_height_for_width (priv->scrollbar,
                                             gtk_list_view_get_scrollbar_width (list_view),
                                             &min_child, &nat_child);
  min_total = MAX (min_total, min_child);
  nat_total = MAX (nat_total, nat_child);

  *minimum = min_total;
  *natural = nat_total;
}

static void
gtk_list_view_size_allocate (GtkWidget      *widget,
			     GtkAllocation  *allocation)
{
  GtkListView *list_view = GTK_LIST_VIEW (widget);
  GtkListViewPrivate *priv = list_view->priv;
  GtkAllocation child_allocation;
  GtkAdjustment *adjustment;
  GtkListViewItem *item;
  gint scrollbar_width, height_total;
  guint n_allocated, n_measured;

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_widget_get_window (widget),
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);

  scrollbar_width = gtk_list_view_get_scrollbar_width (list_view);

  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = allocation->width - scrollbar_width;
  for (item = priv->items;
       item != priv->top;
       item = item->next)
    {
      gtk_widget_set_child_visible (item->widget, FALSE);
    }

  for (n_allocated = 0;
       item != NULL && child_allocation.y < allocation->height;
       item = item->next, n_allocated++)
    {
      gtk_widget_get_preferred_height_for_width (item->widget, child_allocation.width, &child_allocation.height, NULL);
      gtk_widget_size_allocate (item->widget, &child_allocation);
      gtk_widget_set_child_visible (item->widget, TRUE);

      child_allocation.y += child_allocation.height;
    }

  for (; item; item = item->next)
    {
      gtk_widget_set_child_visible (item->widget, FALSE);
    }

  adjustment = gtk_range_get_adjustment (GTK_RANGE (priv->scrollbar));
  gtk_list_view_get_heights (priv->start, priv->end, child_allocation.width, NULL, &height_total, &n_measured);
  if (n_measured)
    {
      priv->default_height = height_total / n_measured;
      gtk_adjustment_configure (adjustment,
                                priv->default_height * priv->top->pos,
                                0,
                                child_allocation.y + priv->default_height * (gtk_list_view_get_n_items (list_view) - n_allocated),
                                allocation->height * 0.1,
                                allocation->height * 0.9,
                                allocation->height);
    }
  else
    {
      priv->default_height = 0;
      gtk_adjustment_configure (adjustment, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    }

  child_allocation.x = allocation->width - scrollbar_width;
  child_allocation.y = 0;
  child_allocation.width = scrollbar_width;
  child_allocation.height = allocation->height;
  gtk_widget_size_allocate (priv->scrollbar, &child_allocation);
}

static void
gtk_list_view_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  /* Make the main, clipping window */
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask = (GDK_EXPOSURE_MASK |
                           GDK_SCROLL_MASK |
                           GDK_SMOOTH_SCROLL_MASK |
                           GDK_POINTER_MOTION_MASK |
                           GDK_BUTTON_PRESS_MASK |
                           GDK_BUTTON_RELEASE_MASK |
                           GDK_KEY_PRESS_MASK |
                           GDK_KEY_RELEASE_MASK) |
    gtk_widget_get_events (widget);
  

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gdk_window_set_user_data (window, widget);
}

static void
gtk_list_view_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkListView *list_view = GTK_LIST_VIEW (container);
  GtkListViewPrivate *priv = list_view->priv;
  GtkListViewItem *item;

  if (!include_internals)
    return;

  for (item = priv->items; item; item = item->next)
    {
      (* callback) (item->widget, callback_data);
    }

  (* callback) (priv->scrollbar, callback_data);
}

static void
gtk_list_view_dispose (GObject *object)
{
  GtkListView *list_view = GTK_LIST_VIEW (object);

  gtk_list_view_set_model (list_view, NULL);
  gtk_list_view_clear (list_view);
  gtk_list_view_set_widget_creation_funcs (list_view,
                                           gtk_list_view_default_widget_create,
                                           gtk_list_view_default_widget_update,
                                           NULL,
                                           NULL);

  G_OBJECT_CLASS (gtk_list_view_parent_class)->dispose (object);
}

static void
gtk_list_view_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  GtkListView *list_view;

  list_view = GTK_LIST_VIEW (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_list_view_set_model (list_view, g_value_get_object (value));
      break;
    case PROP_SELECTION_MODE:
      gtk_list_view_set_selection_mode (list_view, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_list_view_get_property (GObject      *object,
			    guint         prop_id,
			    GValue       *value,
			    GParamSpec   *pspec)
{
  GtkListView *list_view = GTK_LIST_VIEW (object);
  GtkListViewPrivate *priv = list_view->priv;

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, priv->model);
      break;
    case PROP_SELECTION_MODE:
      g_value_set_enum (value, priv->selection_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_list_view_class_init (GtkListViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  gobject_class->dispose = gtk_list_view_dispose;
  gobject_class->set_property = gtk_list_view_set_property;
  gobject_class->get_property = gtk_list_view_get_property;

  /**
   * GtkListView:selection-mode:
   *
   * The ::selection-mode property specifies the selection mode of
   * list view. If the mode is #GTK_SELECTION_MULTIPLE, rubberband selection
   * is enabled, for the other modes, only keyboard selection is possible.
   *
   * Since: 3.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_SELECTION_MODE,
				   g_param_spec_enum ("selection-mode",
						      P_("Selection mode"),
						      P_("The selection mode"),
						      GTK_TYPE_SELECTION_MODE,
						      GTK_SELECTION_SINGLE,
						      G_PARAM_READWRITE));
  /**
   * GtkListView:model:
   *
   * The ::model property provides the #GtkTreeModel that is displayed.
   *
   * Since: 3.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
							P_("List View Model"),
							P_("The model for the list view"),
							GTK_TYPE_TREE_MODEL,
							G_PARAM_READWRITE));
  /**
   * GtkListView:model:
   *
   * The ::builder-xml property containts the #GtkBuilder XML that is used to
   * construct the widget used to display a row.
   *
   * Since: 3.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_BUILDER_XML,
                                   g_param_spec_string ("builder-xml",
							P_("Builder XML"),
							P_("XML used to construct the contents of a row"),
							NULL,
							G_PARAM_READWRITE));

  widget_class->get_request_mode = gtk_list_view_get_request_mode;
  widget_class->get_preferred_width = gtk_list_view_get_preferred_width;
  widget_class->get_preferred_height = gtk_list_view_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_list_view_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_list_view_get_preferred_height_for_width;
  widget_class->realize = gtk_list_view_realize;
  widget_class->size_allocate = gtk_list_view_size_allocate;

  container_class->forall = gtk_list_view_forall;

  g_type_class_add_private (klass, sizeof (GtkListViewPrivate));
}

static GtkListViewItem *
gtk_list_view_find_item_for_y (GtkListView *list_view,
                               gint         value)
{
  GtkListViewPrivate *priv = list_view->priv;
  GtkListViewItem *item;
  int height;

  if (value < 0 || priv->start == NULL)
    return NULL;

  height = priv->top->pos * priv->default_height;
  if (height > value)
    return gtk_list_view_get_item (list_view, value / priv->default_height, TRUE);
  else
    value -= height;

  for (item = priv->top; item && gtk_widget_get_child_visible (item->widget); item = item->next)
    {
      height = gtk_widget_get_allocated_height (item->widget);
      if (height > value)
        return item;
      else
        value -= height;
    }

  if (item)
    {
      height = priv->default_height * (gtk_list_view_get_n_items (list_view) - item->pos);
      if (height > value)
        return gtk_list_view_get_item (list_view, value / priv->default_height + item->pos, TRUE);
    }
  
  return NULL;
}

static gboolean
gtk_list_view_scrollbar_change_value_cb (GtkScrollbar  *scrollbar,
                                         GtkScrollType  scroll_type,
                                         double         new_value,
                                         GtkListView   *list_view)
{
  list_view->priv->top = gtk_list_view_find_item_for_y (list_view, round (gtk_adjustment_get_value (gtk_range_get_adjustment (GTK_RANGE (scrollbar)))));
  gtk_widget_queue_resize (GTK_WIDGET (list_view));

  return FALSE;
}

static void
gtk_list_view_init (GtkListView *list_view)
{
  GtkListViewPrivate *priv;

  list_view->priv = G_TYPE_INSTANCE_GET_PRIVATE (list_view,
                                                 GTK_TYPE_LIST_VIEW,
                                                 GtkListViewPrivate);

  priv = list_view->priv;
  priv->scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, NULL);
  g_signal_connect_after (priv->scrollbar,
                          "change-value",
                          G_CALLBACK (gtk_list_view_scrollbar_change_value_cb),
                          list_view);
  gtk_widget_set_parent (priv->scrollbar, GTK_WIDGET (list_view));
  gtk_widget_show (priv->scrollbar);

  gtk_list_view_set_widget_creation_funcs (list_view,
                                           gtk_list_view_default_widget_create,
                                           gtk_list_view_default_widget_update,
                                           NULL,
                                           NULL);
}

/**
 * gtk_list_view_new:
 *
 * Creates a new #GtkListView widget
 *
 * Return value: A newly created #GtkListView widget
 *
 * Since: 3.6
 **/
GtkWidget *
gtk_list_view_new (void)
{
  return g_object_new (GTK_TYPE_LIST_VIEW, NULL);
}

/**
 * gtk_list_view_new_with_model:
 * @model: The model.
 *
 * Creates a new #GtkListView widget with the model @model.
 *
 * Return value: A newly created #GtkListView widget.
 *
 * Since: 3.6
 **/
GtkWidget *
gtk_list_view_new_with_model (GtkTreeModel *model)
{
  return g_object_new (GTK_TYPE_LIST_VIEW, "model", model, NULL);
}

static void
gtk_list_view_row_changed (GtkTreeModel *model,
                           GtkTreePath  *path,
                           GtkTreeIter  *iter,
                           gpointer      data)
{
  GtkListView *list_view = GTK_LIST_VIEW (data);

  if (!gtk_list_view_contains_path (list_view, path))
    return;
}

static void
gtk_list_view_row_inserted (GtkTreeModel *model,
			    GtkTreePath  *path,
			    GtkTreeIter  *iter,
			    gpointer      data)
{
  GtkListView *list_view = GTK_LIST_VIEW (data);

  if (!gtk_list_view_contains_path (list_view, path))
    return;
}

static void
gtk_list_view_row_deleted (GtkTreeModel *model,
			   GtkTreePath  *path,
			   gpointer      data)
{
  GtkListView *list_view = GTK_LIST_VIEW (data);

  if (!gtk_list_view_contains_path (list_view, path))
    return;
}

static void
gtk_list_view_rows_reordered (GtkTreeModel *model,
			      GtkTreePath  *parent,
			      GtkTreeIter  *iter,
			      gint         *new_order,
			      gpointer      data)
{
  //GtkListView *list_view = GTK_LIST_VIEW (data);

  if (iter != NULL)
    return;
}

/**
 * gtk_list_view_set_model:
 * @list_view: A #GtkListView.
 * @model: (allow-none): The model.
 *
 * Sets the model for a #GtkListView.
 * If the @list_view already has a model set, it will remove
 * it before setting the new model.  If @model is %NULL, then
 * it will unset the old model.
 *
 * Since: 3.6
 **/
void
gtk_list_view_set_model (GtkListView *list_view,
			 GtkTreeModel *model)
{
  GtkListViewPrivate *priv;

  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  priv = list_view->priv;

  if (priv->model == model)
    return;

  if (model)
    {
      g_object_ref (model);
      g_signal_connect (model,
			"row-changed",
			G_CALLBACK (gtk_list_view_row_changed),
			list_view);
      g_signal_connect (model,
			"row-inserted",
			G_CALLBACK (gtk_list_view_row_inserted),
			list_view);
      g_signal_connect (model,
			"row-deleted",
			G_CALLBACK (gtk_list_view_row_deleted),
			list_view);
      g_signal_connect (model,
			"rows-reordered",
			G_CALLBACK (gtk_list_view_rows_reordered),
			list_view);
      
      gtk_list_view_clear (list_view);
    }

  if (priv->model)
    {
      g_signal_handlers_disconnect_by_func (priv->model,
					    gtk_list_view_row_changed,
					    list_view);
      g_signal_handlers_disconnect_by_func (priv->model,
					    gtk_list_view_row_inserted,
					    list_view);
      g_signal_handlers_disconnect_by_func (priv->model,
					    gtk_list_view_row_deleted,
					    list_view);
      g_signal_handlers_disconnect_by_func (priv->model,
					    gtk_list_view_rows_reordered,
					    list_view);

      g_object_unref (list_view->priv->model);
    }

  priv->model = model;

  g_object_notify (G_OBJECT (list_view), "model");

  gtk_widget_queue_resize (GTK_WIDGET (list_view));
}

/**
 * gtk_list_view_get_model:
 * @list_view: a #GtkListView
 *
 * Returns the model the #GtkListView is based on.  Returns %NULL if the
 * model is unset.
 *
 * Return value: (transfer none): A #GtkTreeModel, or %NULL if none is
 *     currently being used.
 *
 * Since: 3.6
 **/
GtkTreeModel *
gtk_list_view_get_model (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), NULL);

  return list_view->priv->model;
}

/**
 * gtk_list_view_set_selection_mode:
 * @list_view: A #GtkListView.
 * @mode: The selection mode
 *
 * Sets the selection mode of the @list_view.
 *
 * Since: 3.6
 **/
void
gtk_list_view_set_selection_mode (GtkListView      *list_view,
				  GtkSelectionMode  mode)
{
  GtkListViewPrivate *priv;

  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  priv = list_view->priv;

  if (priv->selection_mode == mode)
    return;

  priv->selection_mode = mode;

  g_object_notify (G_OBJECT (list_view), "selection-mode");
}

/**
 * gtk_list_view_get_selection_mode:
 * @list_view: A #GtkListView.
 *
 * Gets the selection mode of the @list_view.
 *
 * Return value: the current selection mode
 *
 * Since: 3.6
 **/
GtkSelectionMode
gtk_list_view_get_selection_mode (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), GTK_SELECTION_SINGLE);

  return list_view->priv->selection_mode;
}

gboolean                gtk_list_view_get_visible_range         (GtkListView             *list_view,
                                                                 GtkTreePath            **start_path,
                                                                 GtkTreePath            **end_path);
#if 0
/* GObject vfuncs */
static void             gtk_list_view_cell_layout_init          (GtkCellLayoutIface *iface);
static void             gtk_list_view_dispose                   (GObject            *object);
static GObject         *gtk_list_view_constructor               (GType               type,
								 guint               n_construct_properties,
								 GObjectConstructParam *construct_properties);
static void             gtk_list_view_set_property              (GObject            *object,
								 guint               prop_id,
								 const GValue       *value,
								 GParamSpec         *pspec);
static void             gtk_list_view_get_property              (GObject            *object,
								 guint               prop_id,
								 GValue             *value,
								 GParamSpec         *pspec);
/* GtkWidget vfuncs */
static void             gtk_list_view_destroy                   (GtkWidget          *widget);
static void             gtk_list_view_style_updated             (GtkWidget          *widget);
static void             gtk_list_view_state_flags_changed       (GtkWidget          *widget,
			                                         GtkStateFlags       previous_state);
static gboolean         gtk_list_view_draw                      (GtkWidget          *widget,
                                                                 cairo_t            *cr);
static gboolean         gtk_list_view_motion                    (GtkWidget          *widget,
								 GdkEventMotion     *event);
static gboolean         gtk_list_view_button_press              (GtkWidget          *widget,
								 GdkEventButton     *event);
static gboolean         gtk_list_view_button_release            (GtkWidget          *widget,
								 GdkEventButton     *event);
static gboolean         gtk_list_view_key_press                 (GtkWidget          *widget,
								 GdkEventKey        *event);
static gboolean         gtk_list_view_key_release               (GtkWidget          *widget,
								 GdkEventKey        *event);


/* GtkContainer vfuncs */
static void             gtk_list_view_remove                    (GtkContainer       *container,
								 GtkWidget          *widget);

/* GtkListView vfuncs */
static void             gtk_list_view_real_select_all           (GtkListView        *list_view);
static void             gtk_list_view_real_unselect_all         (GtkListView        *list_view);
static void             gtk_list_view_real_select_cursor_item   (GtkListView        *list_view);
static void             gtk_list_view_real_toggle_cursor_item   (GtkListView        *list_view);
static gboolean         gtk_list_view_real_activate_cursor_item (GtkListView        *list_view);

 /* Internal functions */
static void                 gtk_list_view_set_hadjustment_values         (GtkListView            *list_view);
static void                 gtk_list_view_set_vadjustment_values         (GtkListView            *list_view);
static void                 gtk_list_view_set_hadjustment                (GtkListView            *list_view,
                                                                          GtkAdjustment          *adjustment);
static void                 gtk_list_view_set_vadjustment                (GtkListView            *list_view,
                                                                          GtkAdjustment          *adjustment);
static void                 gtk_list_view_adjustment_changed             (GtkAdjustment          *adjustment,
									  GtkListView            *list_view);
static void                 gtk_list_view_layout                         (GtkListView            *list_view);
static void                 gtk_list_view_paint_item                     (GtkListView            *list_view,
									  cairo_t                *cr,
									  GtkListViewItem        *item,
									  gint                    x,
									  gint                    y,
									  gboolean                draw_focus);
static void                 gtk_list_view_paint_rubberband               (GtkListView            *list_view,
								          cairo_t                *cr);
static void                 gtk_list_view_queue_draw_path                (GtkListView *list_view,
									  GtkTreePath *path);
static void                 gtk_list_view_queue_draw_item                (GtkListView            *list_view,
									  GtkListViewItem        *item);
static void                 gtk_list_view_start_rubberbanding            (GtkListView            *list_view,
                                                                          GdkDevice              *device,
									  gint                    x,
									  gint                    y);
static void                 gtk_list_view_stop_rubberbanding             (GtkListView            *list_view);
static void                 gtk_list_view_update_rubberband_selection    (GtkListView            *list_view);
static gboolean             gtk_list_view_item_hit_test                  (GtkListView            *list_view,
									  GtkListViewItem        *item,
									  gint                    x,
									  gint                    y,
									  gint                    width,
									  gint                    height);
static gboolean             gtk_list_view_unselect_all_internal          (GtkListView            *list_view);
static void                 gtk_list_view_update_rubberband              (gpointer                data);
static void                 gtk_list_view_item_invalidate_size           (GtkListViewItem        *item);
static void                 gtk_list_view_invalidate_sizes               (GtkListView            *list_view);
static void                 gtk_list_view_add_move_binding               (GtkBindingSet          *binding_set,
									  guint                   keyval,
									  guint                   modmask,
									  GtkMovementStep         step,
									  gint                    count);
static gboolean             gtk_list_view_real_move_cursor               (GtkListView            *list_view,
									  GtkMovementStep         step,
									  gint                    count);
static void                 gtk_list_view_move_cursor_up_down            (GtkListView            *list_view,
									  gint                    count);
static void                 gtk_list_view_move_cursor_page_up_down       (GtkListView            *list_view,
									  gint                    count);
static void                 gtk_list_view_move_cursor_left_right         (GtkListView            *list_view,
									  gint                    count);
static void                 gtk_list_view_move_cursor_start_end          (GtkListView            *list_view,
									  gint                    count);
static void                 gtk_list_view_scroll_to_item                 (GtkListView            *list_view,
									  GtkListViewItem        *item);
static gboolean             gtk_list_view_select_all_between             (GtkListView            *list_view,
									  GtkListViewItem        *anchor,
									  GtkListViewItem        *cursor);

static void                 gtk_list_view_ensure_cell_area               (GtkListView            *list_view,
                                                                          GtkCellArea            *cell_area);

static GtkCellArea         *gtk_list_view_cell_layout_get_area           (GtkCellLayout          *layout);

static void                 gtk_list_view_item_selected_changed          (GtkListView            *list_view,
		                                                          GtkListViewItem        *item);

static void                 gtk_list_view_add_editable                   (GtkCellArea            *area,
									  GtkCellRenderer        *renderer,
									  GtkCellEditable        *editable,
									  GdkRectangle           *cell_area,
									  const gchar            *path,
									  GtkListView            *list_view);
static void                 gtk_list_view_remove_editable                (GtkCellArea            *area,
									  GtkCellRenderer        *renderer,
									  GtkCellEditable        *editable,
									  GtkListView            *list_view);
static void                 update_text_cell                             (GtkListView            *list_view);
static void                 update_pixbuf_cell                           (GtkListView            *list_view);

/* Source side drag signals */
static void gtk_list_view_drag_begin       (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void gtk_list_view_drag_end         (GtkWidget        *widget,
                                            GdkDragContext   *context);
static void gtk_list_view_drag_data_get    (GtkWidget        *widget,
                                            GdkDragContext   *context,
                                            GtkSelectionData *selection_data,
                                            guint             info,
                                            guint             time);
static void gtk_list_view_drag_data_delete (GtkWidget        *widget,
                                            GdkDragContext   *context);

/* Target side drag signals */
static void     gtk_list_view_drag_leave         (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  guint             time);
static gboolean gtk_list_view_drag_motion        (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             time);
static gboolean gtk_list_view_drag_drop          (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  guint             time);
static void     gtk_list_view_drag_data_received (GtkWidget        *widget,
                                                  GdkDragContext   *context,
                                                  gint              x,
                                                  gint              y,
                                                  GtkSelectionData *selection_data,
                                                  guint             info,
                                                  guint             time);
static gboolean gtk_list_view_maybe_begin_drag   (GtkListView             *list_view,
					   	  GdkEventMotion          *event);

static void     remove_scroll_timeout            (GtkListView *list_view);

/* GtkBuildable */
static GtkBuildableIface *parent_buildable_iface;
static void     gtk_list_view_buildable_init             (GtkBuildableIface *iface);
static gboolean gtk_list_view_buildable_custom_tag_start (GtkBuildable  *buildable,
							  GtkBuilder    *builder,
							  GObject       *child,
							  const gchar   *tagname,
							  GMarkupParser *parser,
							  gpointer      *data);
static void     gtk_list_view_buildable_custom_tag_end   (GtkBuildable  *buildable,
							  GtkBuilder    *builder,
							  GObject       *child,
							  const gchar   *tagname,
							  gpointer      *data);

static guint list_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkListView, gtk_list_view, GTK_TYPE_CONTAINER,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
						gtk_list_view_cell_layout_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
						gtk_list_view_buildable_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static void
gtk_list_view_class_init (GtkListViewClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;

  binding_set = gtk_binding_set_by_class (klass);

  g_type_class_add_private (klass, sizeof (GtkListViewPrivate));

  gobject_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;

  gobject_class->constructor = gtk_list_view_constructor;
  gobject_class->dispose = gtk_list_view_dispose;
  gobject_class->set_property = gtk_list_view_set_property;
  gobject_class->get_property = gtk_list_view_get_property;

  widget_class->destroy = gtk_list_view_destroy;
  widget_class->style_updated = gtk_list_view_style_updated;
  widget_class->draw = gtk_list_view_draw;
  widget_class->motion_notify_event = gtk_list_view_motion;
  widget_class->button_press_event = gtk_list_view_button_press;
  widget_class->button_release_event = gtk_list_view_button_release;
  widget_class->key_press_event = gtk_list_view_key_press;
  widget_class->key_release_event = gtk_list_view_key_release;
  widget_class->drag_begin = gtk_list_view_drag_begin;
  widget_class->drag_end = gtk_list_view_drag_end;
  widget_class->drag_data_get = gtk_list_view_drag_data_get;
  widget_class->drag_data_delete = gtk_list_view_drag_data_delete;
  widget_class->drag_leave = gtk_list_view_drag_leave;
  widget_class->drag_motion = gtk_list_view_drag_motion;
  widget_class->drag_drop = gtk_list_view_drag_drop;
  widget_class->drag_data_received = gtk_list_view_drag_data_received;
  widget_class->state_flags_changed = gtk_list_view_state_flags_changed;

  container_class->remove = gtk_list_view_remove;

  klass->select_all = gtk_list_view_real_select_all;
  klass->unselect_all = gtk_list_view_real_unselect_all;
  klass->select_cursor_item = gtk_list_view_real_select_cursor_item;
  klass->toggle_cursor_item = gtk_list_view_real_toggle_cursor_item;
  klass->activate_cursor_item = gtk_list_view_real_activate_cursor_item; 
  klass->move_cursor = gtk_list_view_real_move_cursor;
 
  /* Properties */

  /**
   * GtkListView:pixbuf-column:
   *
   * The ::pixbuf-column property contains the number of the model column
   * containing the pixbufs which are displayed. The pixbuf column must be
   * of type #GDK_TYPE_PIXBUF. Setting this property to -1 turns off the
   * display of pixbufs.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_PIXBUF_COLUMN,
				   g_param_spec_int ("pixbuf-column",
						     P_("Pixbuf column"),
						     P_("Model column used to retrieve the list pixbuf from"),
						     -1, G_MAXINT, -1,
						     G_PARAM_READWRITE));

  /**
   * GtkListView:text-column:
   *
   * The ::text-column property contains the number of the model column
   * containing the texts which are displayed. The text column must be
   * of type #G_TYPE_STRING. If this property and the :markup-column
   * property are both set to -1, no texts are displayed.  
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_TEXT_COLUMN,
				   g_param_spec_int ("text-column",
						     P_("Text column"),
						     P_("Model column used to retrieve the text from"),
						     -1, G_MAXINT, -1,
						     G_PARAM_READWRITE));

 
  /**
   * GtkListView:markup-column:
   *
   * The ::markup-column property contains the number of the model column
   * containing markup information to be displayed. The markup column must be
   * of type #G_TYPE_STRING. If this property and the :text-column property
   * are both set to column numbers, it overrides the text column.
   * If both are set to -1, no texts are displayed.  
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_MARKUP_COLUMN,
				   g_param_spec_int ("markup-column",
						     P_("Markup column"),
						     P_("Model column used to retrieve the text if using Pango markup"),
						     -1, G_MAXINT, -1,
						     G_PARAM_READWRITE));
 
 
  /**
   * GtkListView:columns:
   *
   * The columns property contains the number of the columns in which the
   * items should be displayed. If it is -1, the number of columns will
   * be chosen automatically to fill the available area.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_COLUMNS,
				   g_param_spec_int ("columns",
						     P_("Number of columns"),
						     P_("Number of columns to display"),
						     -1, G_MAXINT, -1,
						     G_PARAM_READWRITE));
 

  /**
   * GtkListView:item-width:
   *
   * The item-width property specifies the width to use for each item.
   * If it is set to -1, the list view will automatically determine a
   * suitable item size.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_ITEM_WIDTH,
				   g_param_spec_int ("item-width",
						     P_("Width for each item"),
						     P_("The width used for each item"),
						     -1, G_MAXINT, -1,
						     G_PARAM_READWRITE)); 

  /**
   * GtkListView:spacing:
   *
   * The spacing property specifies the space which is inserted between
   * the cells (i.e. the list and the text) of an item.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing",
						     P_("Spacing"),
						     P_("Space which is inserted between cells of an item"),
						     0, G_MAXINT, 0,
						     G_PARAM_READWRITE));

  /**
   * GtkListView:row-spacing:
   *
   * The row-spacing property specifies the space which is inserted between
   * the rows of the list view.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ROW_SPACING,
                                   g_param_spec_int ("row-spacing",
						     P_("Row Spacing"),
						     P_("Space which is inserted between grid rows"),
						     0, G_MAXINT, 6,
						     G_PARAM_READWRITE));

  /**
   * GtkListView:column-spacing:
   *
   * The column-spacing property specifies the space which is inserted between
   * the columns of the list view.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_COLUMN_SPACING,
                                   g_param_spec_int ("column-spacing",
						     P_("Column Spacing"),
						     P_("Space which is inserted between grid columns"),
						     0, G_MAXINT, 6,
						     G_PARAM_READWRITE));

  /**
   * GtkListView:margin:
   *
   * The margin property specifies the space which is inserted
   * at the edges of the list view.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN,
                                   g_param_spec_int ("margin",
						     P_("Margin"),
						     P_("Space which is inserted at the edges of the list view"),
						     0, G_MAXINT, 6,
						     G_PARAM_READWRITE));

  /**
   * GtkListView:item-orientation:
   *
   * The item-orientation property specifies how the cells (i.e. the list and
   * the text) of the item are positioned relative to each other.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_ITEM_ORIENTATION,
				   g_param_spec_enum ("item-orientation",
						      P_("Item Orientation"),
						      P_("How the text and list of each item are positioned relative to each other"),
						      GTK_TYPE_ORIENTATION,
						      GTK_ORIENTATION_VERTICAL,
						      G_PARAM_READWRITE));

  /**
   * GtkListView:reorderable:
   *
   * The reorderable property specifies if the items can be reordered
   * by DND.
   *
   * Since: 2.8
   */
  g_object_class_install_property (gobject_class,
                                   PROP_REORDERABLE,
                                   g_param_spec_boolean ("reorderable",
							 P_("Reorderable"),
							 P_("View is reorderable"),
							 FALSE,
							 G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_TOOLTIP_COLUMN,
                                     g_param_spec_int ("tooltip-column",
                                                       P_("Tooltip Column"),
                                                       P_("The column in the model containing the tooltip texts for the items"),
                                                       -1,
                                                       G_MAXINT,
                                                       -1,
                                                       G_PARAM_READWRITE));

  /**
   * GtkListView:item-padding:
   *
   * The item-padding property specifies the padding around each
   * of the list view's item.
   *
   * Since: 2.18
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ITEM_PADDING,
                                   g_param_spec_int ("item-padding",
						     P_("Item Padding"),
						     P_("Padding around list view items"),
						     0, G_MAXINT, 6,
						     G_PARAM_READWRITE));

  /**
   * GtkListView:cell-area:
   *
   * The #GtkCellArea used to layout cell renderers for this view.
   *
   * If no area is specified when creating the list view with gtk_list_view_new_with_area()
   * a #GtkCellAreaBox will be used.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
				   PROP_CELL_AREA,
				   g_param_spec_object ("cell-area",
							P_("Cell Area"),
							P_("The GtkCellArea used to layout cells"),
							GTK_TYPE_CELL_AREA,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /* Scrollable interface properties */
  g_object_class_override_property (gobject_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (gobject_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  /* Style properties */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_boxed ("selection-box-color",
                                                               P_("Selection Box Color"),
                                                               P_("Color of the selection box"),
                                                               GDK_TYPE_COLOR,
                                                               GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_uchar ("selection-box-alpha",
                                                               P_("Selection Box Alpha"),
                                                               P_("Opacity of the selection box"),
                                                               0, 0xff,
                                                               0x40,
                                                               GTK_PARAM_READABLE));

  /* Signals */
  /**
   * GtkListView::item-activated:
   * @listview: the object on which the signal is emitted
   * @path: the #GtkTreePath for the activated item
   *
   * The ::item-activated signal is emitted when the method
   * gtk_list_view_item_activated() is called or the user double
   * clicks an item. It is also emitted when a non-editable item
   * is selected and one of the keys: Space, Return or Enter is
   * pressed.
   */
  list_view_signals[ITEM_ACTIVATED] =
    g_signal_new (I_("item-activated"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkListViewClass, item_activated),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TREE_PATH);

  /**
   * GtkListView::selection-changed:
   * @listview: the object on which the signal is emitted
   *
   * The ::selection-changed signal is emitted when the selection
   * (i.e. the set of selected items) changes.
   */
  list_view_signals[SELECTION_CHANGED] =
    g_signal_new (I_("selection-changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkListViewClass, selection_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
 
  /**
   * GtkListView::select-all:
   * @listview: the object on which the signal is emitted
   *
   * A <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user selects all items.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   *
   * The default binding for this signal is Ctrl-a.
   */
  list_view_signals[SELECT_ALL] =
    g_signal_new (I_("select-all"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkListViewClass, select_all),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
 
  /**
   * GtkListView::unselect-all:
   * @listview: the object on which the signal is emitted
   *
   * A <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user unselects all items.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   *
   * The default binding for this signal is Ctrl-Shift-a.
   */
  list_view_signals[UNSELECT_ALL] =
    g_signal_new (I_("unselect-all"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkListViewClass, unselect_all),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkListView::select-cursor-item:
   * @listview: the object on which the signal is emitted
   *
   * A <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user selects the item that is currently
   * focused.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   *
   * There is no default binding for this signal.
   */
  list_view_signals[SELECT_CURSOR_ITEM] =
    g_signal_new (I_("select-cursor-item"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkListViewClass, select_cursor_item),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkListView::toggle-cursor-item:
   * @listview: the object on which the signal is emitted
   *
   * A <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user toggles whether the currently
   * focused item is selected or not. The exact effect of this
   * depend on the selection mode.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control selection
   * programmatically.
   *
   * There is no default binding for this signal is Ctrl-Space.
   */
  list_view_signals[TOGGLE_CURSOR_ITEM] =
    g_signal_new (I_("toggle-cursor-item"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkListViewClass, toggle_cursor_item),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkListView::activate-cursor-item:
   * @listview: the object on which the signal is emitted
   *
   * A <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user activates the currently
   * focused item.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control activation
   * programmatically.
   *
   * The default bindings for this signal are Space, Return and Enter.
   */
  list_view_signals[ACTIVATE_CURSOR_ITEM] =
    g_signal_new (I_("activate-cursor-item"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkListViewClass, activate_cursor_item),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);
 
  /**
   * GtkListView::move-cursor:
   * @listview: the object which received the signal
   * @step: the granularity of the move, as a #GtkMovementStep
   * @count: the number of @step units to move
   *
   * The ::move-cursor signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted when the user initiates a cursor movement.
   *
   * Applications should not connect to it, but may emit it with
   * g_signal_emit_by_name() if they need to control the cursor
   * programmatically.
   *
   * The default bindings for this signal include
   * <itemizedlist>
   * <listitem>Arrow keys which move by individual steps</listitem>
   * <listitem>Home/End keys which move to the first/last item</listitem>
   * <listitem>PageUp/PageDown which move by "pages"</listitem>
   * </itemizedlist>
   *
   * All of these will extend the selection when combined with
   * the Shift modifier.
   */
  list_view_signals[MOVE_CURSOR] =
    g_signal_new (I_("move-cursor"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkListViewClass, move_cursor),
		  NULL, NULL,
		  _gtk_marshal_BOOLEAN__ENUM_INT,
		  G_TYPE_BOOLEAN, 2,
		  GTK_TYPE_MOVEMENT_STEP,
		  G_TYPE_INT);

  /* Key bindings */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
				"select-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
				"unselect-all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, GDK_CONTROL_MASK,
				"toggle-cursor-item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, GDK_CONTROL_MASK,
				"toggle-cursor-item", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_space, 0,
				"activate-cursor-item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Space, 0,
				"activate-cursor-item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Return, 0,
				"activate-cursor-item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_ISO_Enter, 0,
				"activate-cursor-item", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Enter, 0,
				"activate-cursor-item", 0);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Up, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Up, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Down, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Down, 0,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_p, GDK_CONTROL_MASK,
				  GTK_MOVEMENT_DISPLAY_LINES, -1);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_n, GDK_CONTROL_MASK,
				  GTK_MOVEMENT_DISPLAY_LINES, 1);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Home, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Home, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, -1);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_End, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_End, 0,
				  GTK_MOVEMENT_BUFFER_ENDS, 1);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Page_Up, 0,
				  GTK_MOVEMENT_PAGES, -1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Page_Up, 0,
				  GTK_MOVEMENT_PAGES, -1);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Page_Down, 0,
				  GTK_MOVEMENT_PAGES, 1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Page_Down, 0,
				  GTK_MOVEMENT_PAGES, 1);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Right, 0,
				  GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_Left, 0,
				  GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Right, 0,
				  GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  gtk_list_view_add_move_binding (binding_set, GDK_KEY_KP_Left, 0,
				  GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_LIST_VIEW_ACCESSIBLE);
}

static void
gtk_list_view_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = _gtk_cell_layout_buildable_add_child;
  iface->custom_tag_start = gtk_list_view_buildable_custom_tag_start;
  iface->custom_tag_end = gtk_list_view_buildable_custom_tag_end;
}

static void
gtk_list_view_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->get_area = gtk_list_view_cell_layout_get_area;
}

static void
gtk_list_view_init (GtkListView *list_view)
{
  list_view->priv = G_TYPE_INSTANCE_GET_PRIVATE (list_view,
                                                 GTK_TYPE_LIST_VIEW,
                                                 GtkListViewPrivate);

  list_view->priv->width = 0;
  list_view->priv->height = 0;
  list_view->priv->selection_mode = GTK_SELECTION_SINGLE;
  list_view->priv->pressed_button = -1;
  list_view->priv->press_start_x = -1;
  list_view->priv->press_start_y = -1;
  list_view->priv->text_column = -1;
  list_view->priv->markup_column = -1; 
  list_view->priv->pixbuf_column = -1;
  list_view->priv->text_cell = NULL;
  list_view->priv->pixbuf_cell = NULL; 
  list_view->priv->tooltip_column = -1; 

  gtk_widget_set_can_focus (GTK_WIDGET (list_view), TRUE);

  list_view->priv->item_orientation = GTK_ORIENTATION_VERTICAL;

  list_view->priv->columns = -1;
  list_view->priv->item_width = -1;
  list_view->priv->spacing = 0;
  list_view->priv->row_spacing = 6;
  list_view->priv->column_spacing = 6;
  list_view->priv->margin = 6;
  list_view->priv->item_padding = 6;

  list_view->priv->draw_focus = TRUE;
}

/* GObject methods */
/* GtkWidget methods */
static void
gtk_list_view_destroy (GtkWidget *widget)
{
  GtkListView *list_view = GTK_LIST_VIEW (widget);

  gtk_list_view_set_model (list_view, NULL);

  if (list_view->priv->scroll_to_path != NULL)
    {
      gtk_tree_row_reference_free (list_view->priv->scroll_to_path);
      list_view->priv->scroll_to_path = NULL;
    }

  remove_scroll_timeout (list_view);

  if (list_view->priv->hadjustment != NULL)
    {
      g_object_unref (list_view->priv->hadjustment);
      list_view->priv->hadjustment = NULL;
    }

  if (list_view->priv->vadjustment != NULL)
    {
      g_object_unref (list_view->priv->vadjustment);
      list_view->priv->vadjustment = NULL;
    }

  GTK_WIDGET_CLASS (gtk_list_view_parent_class)->destroy (widget);
}

static gint
gtk_list_view_get_n_items (GtkListView *list_view)
{
  GtkListViewPrivate *priv = list_view->priv;

  if (priv->model == NULL)
    return 0;

  return gtk_tree_model_iter_n_children (priv->model, NULL);
}

static void
cell_area_get_preferred_size (GtkListView        *list_view,
                              GtkCellAreaContext *context,
                              GtkOrientation      orientation,
                              gint                for_size,
                              gint               *minimum,
                              gint               *natural)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (for_size > 0)
        gtk_cell_area_get_preferred_width_for_height (list_view->priv->cell_area,
                                                      context,
                                                      GTK_WIDGET (list_view),
                                                      for_size,
                                                      minimum, natural);
      else
        gtk_cell_area_get_preferred_width (list_view->priv->cell_area,
                                           context,
                                           GTK_WIDGET (list_view),
                                           minimum, natural);
    }
  else
    {
      if (for_size > 0)
        gtk_cell_area_get_preferred_height_for_width (list_view->priv->cell_area,
                                                      context,
                                                      GTK_WIDGET (list_view),
                                                      for_size,
                                                      minimum, natural);
      else
        gtk_cell_area_get_preferred_height (list_view->priv->cell_area,
                                            context,
                                            GTK_WIDGET (list_view),
                                            minimum, natural);
    }
}

static void
gtk_list_view_get_preferred_item_size (GtkListView    *list_view,
                                       GtkOrientation  orientation,
                                       gint            for_size,
                                       gint           *minimum,
                                       gint           *natural)
{
  GtkListViewPrivate *priv = list_view->priv;
  GtkCellAreaContext *context;
  GList *items;

  context = gtk_cell_area_create_context (priv->cell_area);

  for_size -= 2 * priv->item_padding;

  if (for_size > 0)
    {
      /* This is necessary for the context to work properly */
      for (items = priv->items; items; items = items->next)
        {
          GtkListViewItem *item = items->data;

          _gtk_list_view_set_cell_data (list_view, item);
          cell_area_get_preferred_size (list_view, context, 1 - orientation, -1, NULL, NULL);
        }
    }

  for (items = priv->items; items; items = items->next)
    {
      GtkListViewItem *item = items->data;

      _gtk_list_view_set_cell_data (list_view, item);
      cell_area_get_preferred_size (list_view, context, orientation, for_size, NULL, NULL);
    }

  cell_area_get_preferred_size (list_view, context, orientation, for_size, minimum, natural);

  if (orientation == GTK_ORIENTATION_HORIZONTAL && priv->item_width >= 0)
    {
      if (minimum)
        *minimum = MAX (*minimum, priv->item_width);
      if (natural)
        *natural = *minimum;
    }

  if (minimum)
    *minimum += 2 * priv->item_padding;
  if (natural)
    *natural += 2 * priv->item_padding;

  g_object_unref (context);
}

static void
gtk_list_view_compute_n_items_for_size (GtkListView    *list_view,
                                        GtkOrientation  orientation,
                                        gint            size,
                                        gint           *min_items,
                                        gint           *max_items)
{
  GtkListViewPrivate *priv = list_view->priv;
  int minimum, natural;

  if (priv->columns > 0)
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (min_items)
            *min_items = priv->columns;
          if (max_items)
            *max_items = priv->columns;
        }
      else
        {
          int n_items = gtk_list_view_get_n_items (list_view);

          if (min_items)
            *min_items = (n_items + priv->columns - 1) / priv->columns;
          if (max_items)
            *max_items = (n_items + priv->columns - 1) / priv->columns;
        }

      return;
    }

  size -= 2 * priv->margin;

  gtk_list_view_get_preferred_item_size (list_view, orientation, -1, &minimum, &natural);
 
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      size += priv->column_spacing;
      minimum += priv->column_spacing;
      natural += priv->column_spacing;
    }
  else
    {
      size += priv->row_spacing;
      minimum += priv->row_spacing;
      natural += priv->row_spacing;
    }

  if (max_items)
    {
      if (size <= minimum)
        *max_items = 1;
      else
        *max_items = size / minimum;
    }

  if (min_items)
    {
      if (size <= natural)
        *min_items = 1;
      else
        *min_items = size / natural;
    }
}

static gboolean
gtk_list_view_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  GtkListView *list_view;
  GList *lists;
  GtkTreePath *path;
  gint dest_index;
  GtkListViewDropPosition dest_pos;
  GtkListViewItem *dest_item = NULL;

  list_view = GTK_LIST_VIEW (widget);

  if (!gtk_cairo_should_draw_window (cr, list_view->priv->bin_window))
    return FALSE;

  cairo_save (cr);

  gtk_cairo_transform_to_window (cr, widget, list_view->priv->bin_window);

  cairo_set_line_width (cr, 1.);

  gtk_list_view_get_drag_dest_item (list_view, &path, &dest_pos);

  if (path)
    {
      dest_index = gtk_tree_path_get_indices (path)[0];
      gtk_tree_path_free (path);
    }
  else
    dest_index = -1;

  for (lists = list_view->priv->items; lists; lists = lists->next)
    {
      GtkListViewItem *item = lists->data;
      GdkRectangle paint_area;

      paint_area.x      = item->cell_area.x      - list_view->priv->item_padding;
      paint_area.y      = item->cell_area.y      - list_view->priv->item_padding;
      paint_area.width  = item->cell_area.width  + list_view->priv->item_padding * 2;
      paint_area.height = item->cell_area.height + list_view->priv->item_padding * 2;
     
      cairo_save (cr);

      cairo_rectangle (cr, paint_area.x, paint_area.y, paint_area.width, paint_area.height);
      cairo_clip (cr);

      if (gdk_cairo_get_clip_rectangle (cr, NULL))
        {
          gtk_list_view_paint_item (list_view, cr, item,
                                    item->cell_area.x, item->cell_area.y,
                                    list_view->priv->draw_focus);

          if (dest_index == item->index)
            dest_item = item;
        }

      cairo_restore (cr);
    }

  if (dest_item &&
      dest_pos != GTK_LIST_VIEW_NO_DROP)
    {
      GtkStyleContext *context;
      GdkRectangle rect = { 0 };

      context = gtk_widget_get_style_context (widget);

      switch (dest_pos)
	{
	case GTK_LIST_VIEW_DROP_INTO:
          rect = dest_item->cell_area;
	  break;
	case GTK_LIST_VIEW_DROP_ABOVE:
          rect.x = dest_item->cell_area.x;
          rect.y = dest_item->cell_area.y - 1;
          rect.width = dest_item->cell_area.width;
          rect.height = 2;
	  break;
	case GTK_LIST_VIEW_DROP_LEFT:
          rect.x = dest_item->cell_area.x - 1;
          rect.y = dest_item->cell_area.y;
          rect.width = 2;
          rect.height = dest_item->cell_area.height;
	  break;
	case GTK_LIST_VIEW_DROP_BELOW:
          rect.x = dest_item->cell_area.x;
          rect.y = dest_item->cell_area.y + dest_item->cell_area.height - 1;
          rect.width = dest_item->cell_area.width;
          rect.height = 2;
	  break;
	case GTK_LIST_VIEW_DROP_RIGHT:
          rect.x = dest_item->cell_area.x + dest_item->cell_area.width - 1;
          rect.y = dest_item->cell_area.y;
          rect.width = 2;
          rect.height = dest_item->cell_area.height;
	case GTK_LIST_VIEW_NO_DROP: ;
	  break;
        }

      gtk_render_focus (context, cr,
                        rect.x, rect.y,
                        rect.width, rect.height);
    }

  if (list_view->priv->doing_rubberband)
    gtk_list_view_paint_rubberband (list_view, cr);

  cairo_restore (cr);

  return GTK_WIDGET_CLASS (gtk_list_view_parent_class)->draw (widget, cr);
}

static gboolean
rubberband_scroll_timeout (gpointer data)
{
  GtkListView *list_view = data;

  gtk_adjustment_set_value (list_view->priv->vadjustment,
                            gtk_adjustment_get_value (list_view->priv->vadjustment) +
                            list_view->priv->scroll_value_diff);

  gtk_list_view_update_rubberband (list_view);
 
  return TRUE;
}

static gboolean
gtk_list_view_motion (GtkWidget      *widget,
		      GdkEventMotion *event)
{
  GtkAllocation allocation;
  GtkListView *list_view;
  gint abs_y;
 
  list_view = GTK_LIST_VIEW (widget);

  gtk_list_view_maybe_begin_drag (list_view, event);

  if (list_view->priv->doing_rubberband)
    {
      gtk_list_view_update_rubberband (widget);
     
      abs_y = event->y - list_view->priv->height *
	(gtk_adjustment_get_value (list_view->priv->vadjustment) /
	 (gtk_adjustment_get_upper (list_view->priv->vadjustment) -
	  gtk_adjustment_get_lower (list_view->priv->vadjustment)));

      gtk_widget_get_allocation (widget, &allocation);

      if (abs_y < 0 || abs_y > allocation.height)
	{
	  if (abs_y < 0)
	    list_view->priv->scroll_value_diff = abs_y;
	  else
	    list_view->priv->scroll_value_diff = abs_y - allocation.height;

	  list_view->priv->event_last_x = event->x;
	  list_view->priv->event_last_y = event->y;

	  if (list_view->priv->scroll_timeout_id == 0)
	    list_view->priv->scroll_timeout_id = gdk_threads_add_timeout (30, rubberband_scroll_timeout,
								list_view);
 	}
      else
	remove_scroll_timeout (list_view);
    }
  else
    {
      GtkListViewItem *item, *last_prelight_item;
      GtkCellRenderer *cell = NULL;

      last_prelight_item = list_view->priv->last_prelight;
      item = _gtk_list_view_get_item_at_coords (list_view,
                                               event->x, event->y,
                                               FALSE,
                                               &cell);

      if (item != NULL)
        {
          item->prelight = TRUE;
          gtk_list_view_queue_draw_item (list_view, item);
        }

      if (last_prelight_item != NULL &&
          last_prelight_item != item)
        {
          last_prelight_item->prelight = FALSE;
          gtk_list_view_queue_draw_item (list_view,
                                         list_view->priv->last_prelight);
        }

      list_view->priv->last_prelight = item;
    }
 
  return TRUE;
}

static void
gtk_list_view_remove (GtkContainer *container,
		      GtkWidget    *widget)
{
  GtkListView *list_view;
  GtkListViewChild *child = NULL;
  GList *tmp_list;

  list_view = GTK_LIST_VIEW (container);
 
  tmp_list = list_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      if (child->widget == widget)
	{
	  gtk_widget_unparent (widget);

	  list_view->priv->children = g_list_remove_link (list_view->priv->children, tmp_list);
	  g_list_free_1 (tmp_list);
	  g_free (child);
	  return;
	}

      tmp_list = tmp_list->next;
    }
}

static void
gtk_list_view_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkListView *list_view;
  GtkListViewChild *child = NULL;
  GList *tmp_list;

  list_view = GTK_LIST_VIEW (container);

  tmp_list = list_view->priv->children;
  while (tmp_list)
    {
      child = tmp_list->data;
      tmp_list = tmp_list->next;

      (* callback) (child->widget, callback_data);
    }
}

static void
gtk_list_view_item_selected_changed (GtkListView      *list_view,
                                     GtkListViewItem  *item)
{
  AtkObject *obj;
  AtkObject *item_obj;

  obj = gtk_widget_get_accessible (GTK_WIDGET (list_view));
  if (obj != NULL)
    {
      item_obj = atk_object_ref_accessible_child (obj, item->index);
      if (item_obj != NULL)
        {
          atk_object_notify_state_change (item_obj, ATK_STATE_SELECTED, item->selected);
          g_object_unref (item_obj);
        }
    }
}

static void
gtk_list_view_add_editable (GtkCellArea            *area,
			    GtkCellRenderer        *renderer,
			    GtkCellEditable        *editable,
			    GdkRectangle           *cell_area,
			    const gchar            *path,
			    GtkListView            *list_view)
{
  GtkListViewChild *child;
  GtkWidget *widget = GTK_WIDGET (editable);
 
  child = g_new (GtkListViewChild, 1);
 
  child->widget      = widget;
  child->area.x      = cell_area->x;
  child->area.y      = cell_area->y;
  child->area.width  = cell_area->width;
  child->area.height = cell_area->height;

  list_view->priv->children = g_list_append (list_view->priv->children, child);

  if (gtk_widget_get_realized (GTK_WIDGET (list_view)))
    gtk_widget_set_parent_window (child->widget, list_view->priv->bin_window);
 
  gtk_widget_set_parent (widget, GTK_WIDGET (list_view));
}

static void
gtk_list_view_remove_editable (GtkCellArea            *area,
			       GtkCellRenderer        *renderer,
			       GtkCellEditable        *editable,
			       GtkListView            *list_view)
{
  GtkTreePath *path;

  if (gtk_widget_has_focus (GTK_WIDGET (editable)))
    gtk_widget_grab_focus (GTK_WIDGET (list_view));
 
  gtk_container_remove (GTK_CONTAINER (list_view),
			GTK_WIDGET (editable)); 

  path = gtk_tree_path_new_from_string (gtk_cell_area_get_current_path_string (area));
  gtk_list_view_queue_draw_path (list_view, path);
  gtk_tree_path_free (path);
}

/**
 * gtk_list_view_set_cursor:
 * @list_view: A #GtkListView
 * @path: A #GtkTreePath
 * @cell: (allow-none): One of the cell renderers of @list_view, or %NULL
 * @start_editing: %TRUE if the specified cell should start being edited.
 *
 * Sets the current keyboard focus to be at @path, and selects it.  This is
 * useful when you want to focus the user's attention on a particular item.
 * If @cell is not %NULL, then focus is given to the cell specified by
 * it. Additionally, if @start_editing is %TRUE, then editing should be
 * started in the specified cell. 
 *
 * This function is often followed by <literal>gtk_widget_grab_focus
 * (list_view)</literal> in order to give keyboard focus to the widget. 
 * Please note that editing can only happen when the widget is realized.
 *
 * Since: 2.8
 **/
void
gtk_list_view_set_cursor (GtkListView     *list_view,
			  GtkTreePath     *path,
			  GtkCellRenderer *cell,
			  gboolean         start_editing)
{
  GtkListViewItem *item = NULL;

  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (path != NULL);
  g_return_if_fail (cell == NULL || GTK_IS_CELL_RENDERER (cell));

  if (list_view->priv->cell_area)
    gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

  if (gtk_tree_path_get_depth (path) == 1)
    item = g_list_nth_data (list_view->priv->items,
			    gtk_tree_path_get_indices(path)[0]);
 
  if (!item)
    return;
 
  _gtk_list_view_set_cursor_item (list_view, item, cell);
  gtk_list_view_scroll_to_path (list_view, path, FALSE, 0.0, 0.0);

  if (start_editing &&
      list_view->priv->cell_area)
    {
      _gtk_list_view_set_cell_data (list_view, item);
      gtk_cell_area_context_allocate (list_view->priv->cell_area_context, item->cell_area.width, item->cell_area.height);
      gtk_cell_area_activate (list_view->priv->cell_area,
                              list_view->priv->cell_area_context,
			      GTK_WIDGET (list_view), &item->cell_area,
			      0 /* XXX flags */, TRUE);
    }
}

/**
 * gtk_list_view_get_cursor:
 * @list_view: A #GtkListView
 * @path: (out) (allow-none): Return location for the current cursor path,
 *        or %NULL
 * @cell: (out) (allow-none): Return location the current focus cell, or %NULL
 *
 * Fills in @path and @cell with the current cursor path and cell.
 * If the cursor isn't currently set, then *@path will be %NULL. 
 * If no cell currently has focus, then *@cell will be %NULL.
 *
 * The returned #GtkTreePath must be freed with gtk_tree_path_free().
 *
 * Return value: %TRUE if the cursor is set.
 *
 * Since: 2.8
 **/
gboolean
gtk_list_view_get_cursor (GtkListView      *list_view,
			  GtkTreePath     **path,
			  GtkCellRenderer **cell)
{
  GtkListViewItem *item;

  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), FALSE);

  item = list_view->priv->cursor_item;

  if (path != NULL)
    {
      if (item != NULL)
	*path = gtk_tree_path_new_from_indices (item->index, -1);
      else
	*path = NULL;
    }

  if (cell != NULL && item != NULL && list_view->priv->cell_area != NULL)
    *cell = gtk_cell_area_get_focus_cell (list_view->priv->cell_area);

  return (item != NULL);
}

static gboolean
gtk_list_view_button_press (GtkWidget      *widget,
			    GdkEventButton *event)
{
  GtkListView *list_view;
  GtkListViewItem *item;
  gboolean dirty = FALSE;
  GtkCellRenderer *cell = NULL, *cursor_cell = NULL;

  list_view = GTK_LIST_VIEW (widget);

  if (event->window != list_view->priv->bin_window)
    return FALSE;

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  if (event->button == GDK_BUTTON_PRIMARY && event->type == GDK_BUTTON_PRESS)
    {
      GdkModifierType extend_mod_mask;
      GdkModifierType modify_mod_mask;

      extend_mod_mask =
        gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_EXTEND_SELECTION);

      modify_mod_mask =
        gtk_widget_get_modifier_mask (widget, GDK_MODIFIER_INTENT_MODIFY_SELECTION);

      item = _gtk_list_view_get_item_at_coords (list_view,
					       event->x, event->y,
					       FALSE,
					       &cell);

      /*
       * We consider only the the cells' area as the item area if the
       * item is not selected, but if it *is* selected, the complete
       * selection rectangle is considered to be part of the item.
       */
      if (item != NULL && (cell != NULL || item->selected))
	{
	  if (cell != NULL)
	    {
	      if (gtk_cell_renderer_is_activatable (cell))
		cursor_cell = cell;
	    }

	  gtk_list_view_scroll_to_item (list_view, item);
	 
	  if (list_view->priv->selection_mode == GTK_SELECTION_NONE)
	    {
	      _gtk_list_view_set_cursor_item (list_view, item, cursor_cell);
	    }
	  else if (list_view->priv->selection_mode == GTK_SELECTION_MULTIPLE &&
		   (event->state & extend_mod_mask))
	    {
	      gtk_list_view_unselect_all_internal (list_view);

	      _gtk_list_view_set_cursor_item (list_view, item, cursor_cell);
	      if (!list_view->priv->anchor_item)
		list_view->priv->anchor_item = item;
	      else
		gtk_list_view_select_all_between (list_view,
						  list_view->priv->anchor_item,
						  item);
	      dirty = TRUE;
	    }
	  else
	    {
	      if ((list_view->priv->selection_mode == GTK_SELECTION_MULTIPLE ||
		  ((list_view->priv->selection_mode == GTK_SELECTION_SINGLE) && item->selected)) &&
		  (event->state & modify_mod_mask))
		{
		  item->selected = !item->selected;
		  gtk_list_view_queue_draw_item (list_view, item);
		  dirty = TRUE;
		}
	      else
		{
		  gtk_list_view_unselect_all_internal (list_view);

		  item->selected = TRUE;
		  gtk_list_view_queue_draw_item (list_view, item);
		  dirty = TRUE;
		}
	      _gtk_list_view_set_cursor_item (list_view, item, cursor_cell);
	      list_view->priv->anchor_item = item;
	    }

	  /* Save press to possibly begin a drag */
	  if (list_view->priv->pressed_button < 0)
	    {
	      list_view->priv->pressed_button = event->button;
	      list_view->priv->press_start_x = event->x;
	      list_view->priv->press_start_y = event->y;
	    }

	  if (!list_view->priv->last_single_clicked)
	    list_view->priv->last_single_clicked = item;

	  /* cancel the current editing, if it exists */
	  gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

	  if (cell != NULL && gtk_cell_renderer_is_activatable (cell))
	    {
	      _gtk_list_view_set_cell_data (list_view, item);
              gtk_cell_area_context_allocate (list_view->priv->cell_area_context, item->cell_area.width, item->cell_area.height);
	      gtk_cell_area_activate (list_view->priv->cell_area,
                                      list_view->priv->cell_area_context,
				      GTK_WIDGET (list_view),
				      &item->cell_area, 0/* XXX flags */, FALSE);
	    }
	}
      else
	{
	  if (list_view->priv->selection_mode != GTK_SELECTION_BROWSE &&
	      !(event->state & modify_mod_mask))
	    {
	      dirty = gtk_list_view_unselect_all_internal (list_view);
	    }
	 
	  if (list_view->priv->selection_mode == GTK_SELECTION_MULTIPLE)
            gtk_list_view_start_rubberbanding (list_view, event->device, event->x, event->y);
	}

      /* don't draw keyboard focus around an clicked-on item */
      list_view->priv->draw_focus = FALSE;
    }

  if (event->button == GDK_BUTTON_PRIMARY && event->type == GDK_2BUTTON_PRESS)
    {
      item = _gtk_list_view_get_item_at_coords (list_view,
					       event->x, event->y,
					       FALSE,
					       NULL);

      if (item && item == list_view->priv->last_single_clicked)
	{
	  GtkTreePath *path;

	  path = gtk_tree_path_new_from_indices (item->index, -1);
	  gtk_list_view_item_activated (list_view, path);
	  gtk_tree_path_free (path);
	}

      list_view->priv->last_single_clicked = NULL;
      list_view->priv->pressed_button = -1;
    }
 
  if (dirty)
    g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0);

  return event->button == GDK_BUTTON_PRIMARY;
}

static gboolean
gtk_list_view_button_release (GtkWidget      *widget,
			      GdkEventButton *event)
{
  GtkListView *list_view;

  list_view = GTK_LIST_VIEW (widget);
 
  if (list_view->priv->pressed_button == event->button)
    list_view->priv->pressed_button = -1;

  gtk_list_view_stop_rubberbanding (list_view);

  remove_scroll_timeout (list_view);

  return TRUE;
}

static gboolean
gtk_list_view_key_press (GtkWidget      *widget,
			 GdkEventKey    *event)
{
  GtkListView *list_view = GTK_LIST_VIEW (widget);

  if (list_view->priv->doing_rubberband)
    {
      if (event->keyval == GDK_KEY_Escape)
	gtk_list_view_stop_rubberbanding (list_view);

      return TRUE;
    }

  return GTK_WIDGET_CLASS (gtk_list_view_parent_class)->key_press_event (widget, event);
}

static gboolean
gtk_list_view_key_release (GtkWidget      *widget,
			   GdkEventKey    *event)
{
  GtkListView *list_view = GTK_LIST_VIEW (widget);

  if (list_view->priv->doing_rubberband)
    return TRUE;

  return GTK_WIDGET_CLASS (gtk_list_view_parent_class)->key_press_event (widget, event);
}

static void
gtk_list_view_update_rubberband (gpointer data)
{
  GtkListView *list_view;
  gint x, y;
  GdkRectangle old_area;
  GdkRectangle new_area;
  GdkRectangle common;
  cairo_region_t *invalid_region;
 
  list_view = GTK_LIST_VIEW (data);

  gdk_window_get_device_position (list_view->priv->bin_window,
                                  list_view->priv->rubberband_device,
                                  &x, &y, NULL);

  x = MAX (x, 0);
  y = MAX (y, 0);

  old_area.x = MIN (list_view->priv->rubberband_x1,
		    list_view->priv->rubberband_x2);
  old_area.y = MIN (list_view->priv->rubberband_y1,
		    list_view->priv->rubberband_y2);
  old_area.width = ABS (list_view->priv->rubberband_x2 -
			list_view->priv->rubberband_x1) + 1;
  old_area.height = ABS (list_view->priv->rubberband_y2 -
			 list_view->priv->rubberband_y1) + 1;
 
  new_area.x = MIN (list_view->priv->rubberband_x1, x);
  new_area.y = MIN (list_view->priv->rubberband_y1, y);
  new_area.width = ABS (x - list_view->priv->rubberband_x1) + 1;
  new_area.height = ABS (y - list_view->priv->rubberband_y1) + 1;

  invalid_region = cairo_region_create_rectangle (&old_area);
  cairo_region_union_rectangle (invalid_region, &new_area);

  gdk_rectangle_intersect (&old_area, &new_area, &common);
  if (common.width > 2 && common.height > 2)
    {
      cairo_region_t *common_region;

      /* make sure the border is invalidated */
      common.x += 1;
      common.y += 1;
      common.width -= 2;
      common.height -= 2;
     
      common_region = cairo_region_create_rectangle (&common);

      cairo_region_subtract (invalid_region, common_region);
      cairo_region_destroy (common_region);
    }
 
  gdk_window_invalidate_region (list_view->priv->bin_window, invalid_region, TRUE);
   
  cairo_region_destroy (invalid_region);

  list_view->priv->rubberband_x2 = x;
  list_view->priv->rubberband_y2 = y; 

  gtk_list_view_update_rubberband_selection (list_view);
}

static void
gtk_list_view_start_rubberbanding (GtkListView  *list_view,
                                   GdkDevice    *device,
				   gint          x,
				   gint          y)
{
  GList *items;

  if (list_view->priv->rubberband_device)
    return;

  for (items = list_view->priv->items; items; items = items->next)
    {
      GtkListViewItem *item = items->data;

      item->selected_before_rubberbanding = item->selected;
    }
 
  list_view->priv->rubberband_x1 = x;
  list_view->priv->rubberband_y1 = y;
  list_view->priv->rubberband_x2 = x;
  list_view->priv->rubberband_y2 = y;

  list_view->priv->doing_rubberband = TRUE;
  list_view->priv->rubberband_device = device;

  gtk_device_grab_add (GTK_WIDGET (list_view), device, TRUE);
}

static void
gtk_list_view_stop_rubberbanding (GtkListView *list_view)
{
  if (!list_view->priv->doing_rubberband)
    return;

  gtk_device_grab_remove (GTK_WIDGET (list_view),
                          list_view->priv->rubberband_device);

  list_view->priv->doing_rubberband = FALSE;
  list_view->priv->rubberband_device = NULL;

  gtk_widget_queue_draw (GTK_WIDGET (list_view));
}

static void
gtk_list_view_update_rubberband_selection (GtkListView *list_view)
{
  GList *items;
  gint x, y, width, height;
  gboolean dirty = FALSE;
 
  x = MIN (list_view->priv->rubberband_x1,
	   list_view->priv->rubberband_x2);
  y = MIN (list_view->priv->rubberband_y1,
	   list_view->priv->rubberband_y2);
  width = ABS (list_view->priv->rubberband_x1 -
	       list_view->priv->rubberband_x2);
  height = ABS (list_view->priv->rubberband_y1 -
		list_view->priv->rubberband_y2);
 
  for (items = list_view->priv->items; items; items = items->next)
    {
      GtkListViewItem *item = items->data;
      gboolean is_in;
      gboolean selected;
     
      is_in = gtk_list_view_item_hit_test (list_view, item,
					   x, y, width, height);

      selected = is_in ^ item->selected_before_rubberbanding;

      if (item->selected != selected)
	{
	  item->selected = selected;
	  dirty = TRUE;
	  gtk_list_view_queue_draw_item (list_view, item);
	}
    }

  if (dirty)
    g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0);
}


typedef struct {
  GdkRectangle hit_rect;
  gboolean     hit;
} HitTestData;

static gboolean
hit_test (GtkCellRenderer    *renderer,
	  const GdkRectangle *cell_area,
	  const GdkRectangle *cell_background,
	  HitTestData        *data)
{
  if (MIN (data->hit_rect.x + data->hit_rect.width, cell_area->x + cell_area->width) -
      MAX (data->hit_rect.x, cell_area->x) > 0 &&
      MIN (data->hit_rect.y + data->hit_rect.height, cell_area->y + cell_area->height) -
      MAX (data->hit_rect.y, cell_area->y) > 0)
    data->hit = TRUE;
 
  return (data->hit != FALSE);
}

static gboolean
gtk_list_view_item_hit_test (GtkListView      *list_view,
			     GtkListViewItem  *item,
			     gint              x,
			     gint              y,
			     gint              width,
			     gint              height)
{
  HitTestData data = { { x, y, width, height }, FALSE };
  GdkRectangle *item_area = &item->cell_area;
  
  if (MIN (x + width, item_area->x + item_area->width) - MAX (x, item_area->x) <= 0 ||
      MIN (y + height, item_area->y + item_area->height) - MAX (y, item_area->y) <= 0)
    return FALSE;

  _gtk_list_view_set_cell_data (list_view, item);
  gtk_cell_area_context_allocate (list_view->priv->cell_area_context, item->cell_area.width, item->cell_area.height);
  gtk_cell_area_foreach_alloc (list_view->priv->cell_area,
                               list_view->priv->cell_area_context,
			       GTK_WIDGET (list_view),
			       item_area, item_area,
			       (GtkCellAllocCallback)hit_test, &data);

  return data.hit;
}

static gboolean
gtk_list_view_unselect_all_internal (GtkListView  *list_view)
{
  gboolean dirty = FALSE;
  GList *items;

  if (list_view->priv->selection_mode == GTK_SELECTION_NONE)
    return FALSE;

  for (items = list_view->priv->items; items; items = items->next)
    {
      GtkListViewItem *item = items->data;

      if (item->selected)
	{
	  item->selected = FALSE;
	  dirty = TRUE;
	  gtk_list_view_queue_draw_item (list_view, item);
	  gtk_list_view_item_selected_changed (list_view, item);
	}
    }

  return dirty;
}


/* GtkListView signals */
static void
gtk_list_view_real_select_all (GtkListView *list_view)
{
  gtk_list_view_select_all (list_view);
}

static void
gtk_list_view_real_unselect_all (GtkListView *list_view)
{
  gtk_list_view_unselect_all (list_view);
}

static void
gtk_list_view_real_select_cursor_item (GtkListView *list_view)
{
  gtk_list_view_unselect_all (list_view);

  if (list_view->priv->cursor_item != NULL)
    _gtk_list_view_select_item (list_view, list_view->priv->cursor_item);
}

static gboolean
gtk_list_view_real_activate_cursor_item (GtkListView *list_view)
{
  GtkTreePath *path;

  if (!list_view->priv->cursor_item)
    return FALSE;

  _gtk_list_view_set_cell_data (list_view, list_view->priv->cursor_item);
  gtk_cell_area_context_allocate (list_view->priv->cell_area_context,
                                  list_view->priv->cursor_item->cell_area.width,
                                  list_view->priv->cursor_item->cell_area.height);
  gtk_cell_area_activate (list_view->priv->cell_area,
                          list_view->priv->cell_area_context,
                          GTK_WIDGET (list_view),
                          &list_view->priv->cursor_item->cell_area,
                          0 /* XXX flags */,
                          FALSE);

  path = gtk_tree_path_new_from_indices (list_view->priv->cursor_item->index, -1);
  gtk_list_view_item_activated (list_view, path);
  gtk_tree_path_free (path);

  return TRUE;
}

static void
gtk_list_view_real_toggle_cursor_item (GtkListView *list_view)
{
  if (!list_view->priv->cursor_item)
    return;

  switch (list_view->priv->selection_mode)
    {
    case GTK_SELECTION_NONE:
      break;
    case GTK_SELECTION_BROWSE:
      _gtk_list_view_select_item (list_view, list_view->priv->cursor_item);
      break;
    case GTK_SELECTION_SINGLE:
      if (list_view->priv->cursor_item->selected)
	_gtk_list_view_unselect_item (list_view, list_view->priv->cursor_item);
      else
	_gtk_list_view_select_item (list_view, list_view->priv->cursor_item);
      break;
    case GTK_SELECTION_MULTIPLE:
      list_view->priv->cursor_item->selected = !list_view->priv->cursor_item->selected;
      g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0);
     
      gtk_list_view_item_selected_changed (list_view, list_view->priv->cursor_item);     
      gtk_list_view_queue_draw_item (list_view, list_view->priv->cursor_item);
      break;
    }
}

static void
gtk_list_view_set_hadjustment_values (GtkListView *list_view)
{
  GtkAllocation  allocation;
  GtkAdjustment *adj = list_view->priv->hadjustment;
  gdouble old_page_size;
  gdouble old_upper;
  gdouble old_value;
  gdouble new_value;
  gdouble new_upper;

  gtk_widget_get_allocation (GTK_WIDGET (list_view), &allocation);

  old_value = gtk_adjustment_get_value (adj);
  old_upper = gtk_adjustment_get_upper (adj);
  old_page_size = gtk_adjustment_get_page_size (adj);
  new_upper = MAX (allocation.width, list_view->priv->width);

  if (gtk_widget_get_direction (GTK_WIDGET (list_view)) == GTK_TEXT_DIR_RTL)
    {
      /* Make sure no scrolling occurs for RTL locales also (if possible) */
      /* Quick explanation:
       *   In LTR locales, leftmost portion of visible rectangle should stay
       *   fixed, which means left edge of scrollbar thumb should remain fixed
       *   and thus adjustment's value should stay the same.
       *
       *   In RTL locales, we want to keep rightmost portion of visible
       *   rectangle fixed. This means right edge of thumb should remain fixed.
       *   In this case, upper - value - page_size should remain constant.
       */
      new_value = (new_upper - allocation.width) -
                  (old_upper - old_value - old_page_size);
      new_value = CLAMP (new_value, 0, new_upper - allocation.width);
    }
  else
    new_value = CLAMP (old_value, 0, new_upper - allocation.width);

  gtk_adjustment_configure (adj,
                            new_value,
                            0.0,
                            new_upper,
                            allocation.width * 0.1,
                            allocation.width * 0.9,
                            allocation.width);
}

static void
gtk_list_view_set_vadjustment_values (GtkListView *list_view)
{
  GtkAllocation  allocation;
  GtkAdjustment *adj = list_view->priv->vadjustment;

  gtk_widget_get_allocation (GTK_WIDGET (list_view), &allocation);

  gtk_adjustment_configure (adj,
                            gtk_adjustment_get_value (adj),
                            0.0,
                            MAX (allocation.height, list_view->priv->height),
                            allocation.height * 0.1,
                            allocation.height * 0.9,
                            allocation.height);
}

static void
gtk_list_view_set_hadjustment (GtkListView   *list_view,
                               GtkAdjustment *adjustment)
{
  GtkListViewPrivate *priv = list_view->priv;

  if (adjustment && priv->hadjustment == adjustment)
    return;

  if (priv->hadjustment != NULL)
    {
      g_signal_handlers_disconnect_matched (priv->hadjustment,
                                            G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL, list_view);
      g_object_unref (priv->hadjustment);
    }

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_list_view_adjustment_changed), list_view);
  priv->hadjustment = g_object_ref_sink (adjustment);
  gtk_list_view_set_hadjustment_values (list_view);

  g_object_notify (G_OBJECT (list_view), "hadjustment");
}

static void
gtk_list_view_set_vadjustment (GtkListView   *list_view,
                               GtkAdjustment *adjustment)
{
  GtkListViewPrivate *priv = list_view->priv;

  if (adjustment && priv->vadjustment == adjustment)
    return;

  if (priv->vadjustment != NULL)
    {
      g_signal_handlers_disconnect_matched (priv->vadjustment,
                                            G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL, list_view);
      g_object_unref (priv->vadjustment);
    }

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0);

  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (gtk_list_view_adjustment_changed), list_view);
  priv->vadjustment = g_object_ref_sink (adjustment);
  gtk_list_view_set_vadjustment_values (list_view);

  g_object_notify (G_OBJECT (list_view), "vadjustment");
}

static void
gtk_list_view_adjustment_changed (GtkAdjustment *adjustment,
                                  GtkListView   *list_view)
{
  GtkListViewPrivate *priv = list_view->priv;

  if (gtk_widget_get_realized (GTK_WIDGET (list_view)))
    {
      gdk_window_move (priv->bin_window,
                       - gtk_adjustment_get_value (priv->hadjustment),
                       - gtk_adjustment_get_value (priv->vadjustment));

      if (list_view->priv->doing_rubberband)
        gtk_list_view_update_rubberband (GTK_WIDGET (list_view));
     
      _gtk_list_view_accessible_adjustment_changed (list_view);
    }
}

static void
adjust_wrap_width (GtkListView *list_view)
{
  if (list_view->priv->text_cell)
    {
      gint wrap_width = 50;

      /* Here we go with the same old guess, try the list size and set double
       * the size of the first list found in the list, naive but works much
       * of the time */
      if (list_view->priv->items && list_view->priv->pixbuf_cell)
	{
	  _gtk_list_view_set_cell_data (list_view, list_view->priv->items->data);
	  gtk_cell_renderer_get_preferred_width (list_view->priv->pixbuf_cell,
						 GTK_WIDGET (list_view),
						 &wrap_width, NULL);
	 
	  wrap_width = MAX (wrap_width * 2, 50);
	}
     
      g_object_set (list_view->priv->text_cell, "wrap-width", wrap_width, NULL);
      g_object_set (list_view->priv->text_cell, "width", wrap_width, NULL);
    }
}

static gint
compare_sizes (gconstpointer p1,
	       gconstpointer p2,
               gpointer      unused)
{
  return GPOINTER_TO_INT (((const GtkRequestedSize *) p1)->data)
       - GPOINTER_TO_INT (((const GtkRequestedSize *) p2)->data);
}

static void
gtk_list_view_layout (GtkListView *list_view)
{
  GtkListViewPrivate *priv = list_view->priv;
  GtkWidget *widget = GTK_WIDGET (list_view);
  GList *items;
  gint item_width;
  gint n_columns, n_rows, n_items;
  gint col, row;
  GtkRequestedSize *sizes;

  n_items = gtk_list_view_get_n_items (list_view);

  /* Update the wrap width for the text cell before going and requesting sizes */
  if (n_items)
    adjust_wrap_width (list_view);

  gtk_list_view_compute_n_items_for_size (list_view,
                                          GTK_ORIENTATION_HORIZONTAL,
                                          gtk_widget_get_allocated_width (widget),
                                          NULL,
                                          &n_columns);
  n_rows = (n_items + n_columns - 1) / n_columns;

  if (n_columns <= 1)
    {
      /* We might need vertical scrolling here */
      gtk_list_view_get_preferred_item_size (list_view, GTK_ORIENTATION_HORIZONTAL, -1, &item_width, NULL);
      item_width += 2 * priv->item_padding + 2 * priv->margin;
      priv->width = MAX (item_width, gtk_widget_get_allocated_width (widget));
    }
  else
    {
      priv->width = gtk_widget_get_allocated_width (widget);
    }

  item_width = (priv->width - 2 * priv->margin + priv->column_spacing) / n_columns;
  item_width -= priv->column_spacing;
  item_width -= 2 * priv->item_padding;

  gtk_cell_area_context_reset (priv->cell_area_context);
  gtk_cell_area_context_allocate (priv->cell_area_context, item_width, -1);
  /* because layouting is complicated. We designed an API
   * that is O(N²) and nonsensical.
   * And we're proud of it. */
  for (items = priv->items; items; items = items->next)
    {
      _gtk_list_view_set_cell_data (list_view, items->data);
      gtk_cell_area_get_preferred_width (priv->cell_area,
                                         priv->cell_area_context,
                                         widget,
                                         NULL, NULL);
    }

  sizes = g_newa (GtkRequestedSize, n_rows);
  items = priv->items;
  priv->height = priv->margin;

  /* Collect the heights for all rows */
  for (row = 0; row < n_rows; row++)
    {
      for (col = 0; col < n_columns && items; col++, items = items->next)
        {
          GtkListViewItem *item = items->data;

          _gtk_list_view_set_cell_data (list_view, item);
          gtk_cell_area_get_preferred_height_for_width (priv->cell_area,
                                                        priv->cell_area_context,
                                                        widget,
                                                        item_width,
                                                        NULL, NULL);
        }
     
      sizes[row].data = GINT_TO_POINTER (row);
      gtk_cell_area_context_get_preferred_height_for_width (priv->cell_area_context,
                                                            item_width,
                                                            &sizes[row].minimum_size,
                                                            &sizes[row].natural_size);
      priv->height += sizes[row].minimum_size + 2 * priv->item_padding + priv->row_spacing;
    }

  priv->height -= priv->row_spacing;
  priv->height += priv->margin;
  priv->height = MIN (priv->height, gtk_widget_get_allocated_height (widget));

  gtk_distribute_natural_allocation (gtk_widget_get_allocated_height (widget) - priv->height,
                                     n_rows,
                                     sizes);

  /* Actually allocate the rows */
  g_qsort_with_data (sizes, n_rows, sizeof (GtkRequestedSize), compare_sizes, NULL);
 
  items = priv->items;
  priv->height = priv->margin;

  for (row = 0; row < n_rows; row++)
    {
      priv->height += priv->item_padding;

      for (col = 0; col < n_columns && items; col++, items = items->next)
        {
          GtkListViewItem *item = items->data;

          item->cell_area.x = priv->margin + (col * 2 + 1) * priv->item_padding + col * (priv->column_spacing + item_width);
          item->cell_area.width = item_width;
          item->cell_area.y = priv->height;
          item->cell_area.height = sizes[row].minimum_size;
          item->row = row;
          item->col = col;
        }
     
      priv->height += sizes[row].minimum_size + priv->item_padding + priv->row_spacing;
    }

  priv->height -= priv->row_spacing;
  priv->height += priv->margin;
  priv->height = MAX (priv->height, gtk_widget_get_allocated_height (widget));
}

static void
gtk_list_view_invalidate_sizes (GtkListView *list_view)
{
  /* Clear all item sizes */
  g_list_foreach (list_view->priv->items,
		  (GFunc)gtk_list_view_item_invalidate_size, NULL);

  /* Re-layout the items */
  gtk_widget_queue_resize (GTK_WIDGET (list_view));
}

static void
gtk_list_view_item_invalidate_size (GtkListViewItem *item)
{
  item->cell_area.width = -1;
  item->cell_area.height = -1;
}

static void
gtk_list_view_paint_item (GtkListView     *list_view,
                          cairo_t         *cr,
                          GtkListViewItem *item,
                          gint             x,
                          gint             y,
                          gboolean         draw_focus)
{
  GdkRectangle cell_area;
  GtkStateFlags state = 0;
  GtkCellRendererState flags = 0;
  GtkStyleContext *style_context;
  GtkWidget *widget = GTK_WIDGET (list_view);
  GtkListViewPrivate *priv = list_view->priv;

  if (priv->model == NULL)
    return;

  _gtk_list_view_set_cell_data (list_view, item);

  style_context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_save (style_context);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_add_class (style_context, GTK_STYLE_CLASS_CELL);

  state &= ~(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_PRELIGHT);

  if (item->selected)
    {
      if ((state & GTK_STATE_FLAG_FOCUSED) &&
          item == list_view->priv->cursor_item)
        {
          flags |= GTK_CELL_RENDERER_FOCUSED;
        }

      state |= GTK_STATE_FLAG_SELECTED;
      flags |= GTK_CELL_RENDERER_SELECTED;
    }

  if (item->prelight)
    {
      state |= GTK_STATE_FLAG_PRELIGHT;
      flags |= GTK_CELL_RENDERER_PRELIT;
    }

  gtk_style_context_set_state (style_context, state);

  if (item->selected)
    {
      gtk_render_background (style_context, cr,
                             x - list_view->priv->item_padding,
                             y - list_view->priv->item_padding,
                             item->cell_area.width  + list_view->priv->item_padding * 2,
                             item->cell_area.height + list_view->priv->item_padding * 2);
      gtk_render_frame (style_context, cr,
                        x - list_view->priv->item_padding,
                        y - list_view->priv->item_padding,
                        item->cell_area.width  + list_view->priv->item_padding * 2,
                        item->cell_area.height + list_view->priv->item_padding * 2);
    }

  cell_area.x      = x;
  cell_area.y      = y;
  cell_area.width  = item->cell_area.width;
  cell_area.height = item->cell_area.height;

  gtk_cell_area_context_allocate (priv->cell_area_context, item->cell_area.width, item->cell_area.height);
  gtk_cell_area_render (priv->cell_area, priv->cell_area_context,
                        widget, cr, &cell_area, &cell_area, flags,
                        draw_focus);

  gtk_style_context_restore (style_context);
}

static void
gtk_list_view_paint_rubberband (GtkListView     *list_view,
				cairo_t         *cr)
{
  GtkStyleContext *context;
  GdkRectangle rect;

  cairo_save (cr);

  rect.x = MIN (list_view->priv->rubberband_x1, list_view->priv->rubberband_x2);
  rect.y = MIN (list_view->priv->rubberband_y1, list_view->priv->rubberband_y2);
  rect.width = ABS (list_view->priv->rubberband_x1 - list_view->priv->rubberband_x2) + 1;
  rect.height = ABS (list_view->priv->rubberband_y1 - list_view->priv->rubberband_y2) + 1;

  context = gtk_widget_get_style_context (GTK_WIDGET (list_view));

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_RUBBERBAND);

  gdk_cairo_rectangle (cr, &rect);
  cairo_clip (cr);

  gtk_render_background (context, cr,
                         rect.x, rect.y,
                         rect.width, rect.height);
  gtk_render_frame (context, cr,
                    rect.x, rect.y,
                    rect.width, rect.height);

  gtk_style_context_restore (context);
  cairo_restore (cr);
}

static void
gtk_list_view_queue_draw_path (GtkListView *list_view,
			       GtkTreePath *path)
{
  GList *l;
  gint index;

  index = gtk_tree_path_get_indices (path)[0];

  for (l = list_view->priv->items; l; l = l->next)
    {
      GtkListViewItem *item = l->data;

      if (item->index == index)
	{
	  gtk_list_view_queue_draw_item (list_view, item);
	  break;
	}
    }
}

static void
gtk_list_view_queue_draw_item (GtkListView     *list_view,
			       GtkListViewItem *item)
{
  GdkRectangle  rect;
  GdkRectangle *item_area = &item->cell_area;

  rect.x      = item_area->x - list_view->priv->item_padding;
  rect.y      = item_area->y - list_view->priv->item_padding;
  rect.width  = item_area->width  + list_view->priv->item_padding * 2;
  rect.height = item_area->height + list_view->priv->item_padding * 2;

  if (list_view->priv->bin_window)
    gdk_window_invalidate_rect (list_view->priv->bin_window, &rect, TRUE);
}

void
_gtk_list_view_set_cursor_item (GtkListView     *list_view,
                                GtkListViewItem *item,
                                GtkCellRenderer *cursor_cell)
{
  AtkObject *obj;
  AtkObject *item_obj;
  AtkObject *cursor_item_obj;

  /* When hitting this path from keynav, the focus cell is
   * already set, we dont need to notify the atk object
   * but we still need to queue the draw here (in the case
   * that the focus cell changes but not the cursor item).
   */
  gtk_list_view_queue_draw_item (list_view, item);

  if (list_view->priv->cursor_item == item &&
      (cursor_cell == NULL || cursor_cell == gtk_cell_area_get_focus_cell (list_view->priv->cell_area)))
    return;

  obj = gtk_widget_get_accessible (GTK_WIDGET (list_view));
  if (list_view->priv->cursor_item != NULL)
    {
      gtk_list_view_queue_draw_item (list_view, list_view->priv->cursor_item);
      if (obj != NULL)
        {
          cursor_item_obj = atk_object_ref_accessible_child (obj, list_view->priv->cursor_item->index);
          if (cursor_item_obj != NULL)
            atk_object_notify_state_change (cursor_item_obj, ATK_STATE_FOCUSED, FALSE);
        }
    }
  list_view->priv->cursor_item = item;

  if (cursor_cell)
    gtk_cell_area_set_focus_cell (list_view->priv->cell_area, cursor_cell);
  else
    {
      /* Make sure there is a cell in focus initially */
      if (!gtk_cell_area_get_focus_cell (list_view->priv->cell_area))
	gtk_cell_area_focus (list_view->priv->cell_area, GTK_DIR_TAB_FORWARD);
    }
 
  /* Notify that accessible focus object has changed */
  item_obj = atk_object_ref_accessible_child (obj, item->index);

  if (item_obj != NULL)
    {
      atk_focus_tracker_notify (item_obj);
      atk_object_notify_state_change (item_obj, ATK_STATE_FOCUSED, TRUE);
      g_object_unref (item_obj);
    }
}


static GtkListViewItem *
gtk_list_view_item_new (void)
{
  GtkListViewItem *item;

  item = g_slice_new0 (GtkListViewItem);

  item->cell_area.width  = -1;
  item->cell_area.height = -1;
 
  return item;
}

GtkListViewItem *
_gtk_list_view_get_item_at_coords (GtkListView          *list_view,
                                   gint                  x,
                                   gint                  y,
                                   gboolean              only_in_cell,
                                   GtkCellRenderer     **cell_at_pos)
{
  GList *items;

  if (cell_at_pos)
    *cell_at_pos = NULL;

  for (items = list_view->priv->items; items; items = items->next)
    {
      GtkListViewItem *item = items->data;
      GdkRectangle    *item_area = &item->cell_area;

      if (x >= item_area->x - list_view->priv->column_spacing/2 &&
	  x <= item_area->x + item_area->width + list_view->priv->column_spacing/2 &&
	  y >= item_area->y - list_view->priv->row_spacing/2 &&
	  y <= item_area->y + item_area->height + list_view->priv->row_spacing/2)
	{
	  if (only_in_cell || cell_at_pos)
	    {
	      GtkCellRenderer *cell = NULL;

	      _gtk_list_view_set_cell_data (list_view, item);

	      if (x >= item_area->x && x <= item_area->x + item_area->width &&
		  y >= item_area->y && y <= item_area->y + item_area->height)
                {
                  gtk_cell_area_context_allocate (list_view->priv->cell_area_context, item->cell_area.width, item->cell_area.height);
                  cell = gtk_cell_area_get_cell_at_position (list_view->priv->cell_area,
                                                             list_view->priv->cell_area_context,
                                                             GTK_WIDGET (list_view),
                                                             item_area,
                                                             x, y, NULL);
                }

	      if (cell_at_pos)
		*cell_at_pos = cell;

	      if (only_in_cell)
		return cell != NULL ? item : NULL;
	      else
		return item;
	    }
	  return item;
	}
    }
  return NULL;
}

void
_gtk_list_view_select_item (GtkListView      *list_view,
                            GtkListViewItem  *item)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (item != NULL);

  if (item->selected)
    return;
 
  if (list_view->priv->selection_mode == GTK_SELECTION_NONE)
    return;
  else if (list_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    gtk_list_view_unselect_all_internal (list_view);

  item->selected = TRUE;

  gtk_list_view_item_selected_changed (list_view, item);
  g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0);

  gtk_list_view_queue_draw_item (list_view, item);
}


void
_gtk_list_view_unselect_item (GtkListView      *list_view,
                              GtkListViewItem  *item)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (item != NULL);

  if (!item->selected)
    return;
 
  if (list_view->priv->selection_mode == GTK_SELECTION_NONE ||
      list_view->priv->selection_mode == GTK_SELECTION_BROWSE)
    return;
 
  item->selected = FALSE;

  gtk_list_view_item_selected_changed (list_view, item);
  g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0);

  gtk_list_view_queue_draw_item (list_view, item);
}

static void
verify_items (GtkListView *list_view)
{
  GList *items;
  int i = 0;

  for (items = list_view->priv->items; items; items = items->next)
    {
      GtkListViewItem *item = items->data;

      if (item->index != i)
	g_error ("List item does not match its index: "
		 "item index %d and list index %d\n", item->index, i);

      i++;
    }
}

static void
gtk_list_view_add_move_binding (GtkBindingSet  *binding_set,
				guint           keyval,
				guint           modmask,
				GtkMovementStep step,
				gint            count)
{
 
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
                                I_("move-cursor"), 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_SHIFT_MASK,
                                "move-cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

  if ((modmask & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
   return;

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                "move-cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);

  gtk_binding_entry_add_signal (binding_set, keyval, GDK_CONTROL_MASK,
                                "move-cursor", 2,
                                G_TYPE_ENUM, step,
                                G_TYPE_INT, count);
}

static gboolean
gtk_list_view_real_move_cursor (GtkListView     *list_view,
				GtkMovementStep  step,
				gint             count)
{
  GdkModifierType state;

  g_return_val_if_fail (GTK_LIST_VIEW (list_view), FALSE);
  g_return_val_if_fail (step == GTK_MOVEMENT_LOGICAL_POSITIONS ||
			step == GTK_MOVEMENT_VISUAL_POSITIONS ||
			step == GTK_MOVEMENT_DISPLAY_LINES ||
			step == GTK_MOVEMENT_PAGES ||
			step == GTK_MOVEMENT_BUFFER_ENDS, FALSE);

  if (!gtk_widget_has_focus (GTK_WIDGET (list_view)))
    return FALSE;

  gtk_cell_area_stop_editing (list_view->priv->cell_area, FALSE);
  gtk_widget_grab_focus (GTK_WIDGET (list_view));

  if (gtk_get_current_event_state (&state))
    {
      GdkModifierType extend_mod_mask;
      GdkModifierType modify_mod_mask;

      extend_mod_mask =
        gtk_widget_get_modifier_mask (GTK_WIDGET (list_view),
                                      GDK_MODIFIER_INTENT_EXTEND_SELECTION);
      modify_mod_mask =
        gtk_widget_get_modifier_mask (GTK_WIDGET (list_view),
                                      GDK_MODIFIER_INTENT_MODIFY_SELECTION);

      if ((state & modify_mod_mask) == modify_mod_mask)
        list_view->priv->modify_selection_pressed = TRUE;
      if ((state & extend_mod_mask) == extend_mod_mask)
        list_view->priv->extend_selection_pressed = TRUE;
    }
  /* else we assume not pressed */

  switch (step)
    {
    case GTK_MOVEMENT_LOGICAL_POSITIONS:
    case GTK_MOVEMENT_VISUAL_POSITIONS:
      gtk_list_view_move_cursor_left_right (list_view, count);
      break;
    case GTK_MOVEMENT_DISPLAY_LINES:
      gtk_list_view_move_cursor_up_down (list_view, count);
      break;
    case GTK_MOVEMENT_PAGES:
      gtk_list_view_move_cursor_page_up_down (list_view, count);
      break;
    case GTK_MOVEMENT_BUFFER_ENDS:
      gtk_list_view_move_cursor_start_end (list_view, count);
      break;
    default:
      g_assert_not_reached ();
    }

  list_view->priv->modify_selection_pressed = FALSE;
  list_view->priv->extend_selection_pressed = FALSE;

  list_view->priv->draw_focus = TRUE;

  return TRUE;
}

static GtkListViewItem *
find_item (GtkListView     *list_view,
	   GtkListViewItem *current,
	   gint             row_ofs,
	   gint             col_ofs)
{
  gint row, col;
  GList *items;
  GtkListViewItem *item;

  /* FIXME: this could be more efficient
   */
  row = current->row + row_ofs;
  col = current->col + col_ofs;

  for (items = list_view->priv->items; items; items = items->next)
    {
      item = items->data;
      if (item->row == row && item->col == col)
	return item;
    }
 
  return NULL;
}

static GtkListViewItem *
find_item_page_up_down (GtkListView     *list_view,
			GtkListViewItem *current,
			gint             count)
{
  GList *item, *next;
  gint y, col;
 
  col = current->col;
  y = current->cell_area.y + count * gtk_adjustment_get_page_size (list_view->priv->vadjustment);

  item = g_list_find (list_view->priv->items, current);
  if (count > 0)
    {
      while (item)
	{
	  for (next = item->next; next; next = next->next)
	    {
	      if (((GtkListViewItem *)next->data)->col == col)
		break;
	    }
	  if (!next || ((GtkListViewItem *)next->data)->cell_area.y > y)
	    break;

	  item = next;
	}
    }
  else
    {
      while (item)
	{
	  for (next = item->prev; next; next = next->prev)
	    {
	      if (((GtkListViewItem *)next->data)->col == col)
		break;
	    }
	  if (!next || ((GtkListViewItem *)next->data)->cell_area.y < y)
	    break;

	  item = next;
	}
    }

  if (item)
    return item->data;

  return NULL;
}

static gboolean
gtk_list_view_select_all_between (GtkListView     *list_view,
				  GtkListViewItem *anchor,
				  GtkListViewItem *cursor)
{
  GList *items;
  GtkListViewItem *item;
  gint row1, row2, col1, col2;
  gboolean dirty = FALSE;
 
  if (anchor->row < cursor->row)
    {
      row1 = anchor->row;
      row2 = cursor->row;
    }
  else
    {
      row1 = cursor->row;
      row2 = anchor->row;
    }

  if (anchor->col < cursor->col)
    {
      col1 = anchor->col;
      col2 = cursor->col;
    }
  else
    {
      col1 = cursor->col;
      col2 = anchor->col;
    }

  for (items = list_view->priv->items; items; items = items->next)
    {
      item = items->data;

      if (row1 <= item->row && item->row <= row2 &&
	  col1 <= item->col && item->col <= col2)
	{
	  if (!item->selected)
	    {
	      dirty = TRUE;
	      item->selected = TRUE;
	      gtk_list_view_item_selected_changed (list_view, item);
	    }
	  gtk_list_view_queue_draw_item (list_view, item);
	}
    }

  return dirty;
}

static void
gtk_list_view_move_cursor_up_down (GtkListView *list_view,
				   gint         count)
{
  GtkListViewItem *item;
  GtkCellRenderer *cell = NULL;
  gboolean dirty = FALSE;
  gint step;
  GtkDirectionType direction;

  if (!gtk_widget_has_focus (GTK_WIDGET (list_view)))
    return;

  direction = count < 0 ? GTK_DIR_UP : GTK_DIR_DOWN;

  if (!list_view->priv->cursor_item)
    {
      GList *list;

      if (count > 0)
	list = list_view->priv->items;
      else
	list = g_list_last (list_view->priv->items);

      if (list)
        {
          item = list->data;

          /* Give focus to the first cell initially */
          _gtk_list_view_set_cell_data (list_view, item);
          gtk_cell_area_focus (list_view->priv->cell_area, direction);
        }
      else
        {
          item = NULL;
        }
    }
  else
    {
      item = list_view->priv->cursor_item;
      step = count > 0 ? 1 : -1;     

      /* Save the current focus cell in case we hit the edge */
      cell = gtk_cell_area_get_focus_cell (list_view->priv->cell_area);

      while (item)
	{
	  _gtk_list_view_set_cell_data (list_view, item);

	  if (gtk_cell_area_focus (list_view->priv->cell_area, direction))
	    break;

	  item = find_item (list_view, item, step, 0);
	}
    }

  if (!item)
    {
      if (!gtk_widget_keynav_failed (GTK_WIDGET (list_view), direction))
        {
          GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (list_view));
          if (toplevel)
            gtk_widget_child_focus (toplevel,
                                    direction == GTK_DIR_UP ?
                                    GTK_DIR_TAB_BACKWARD :
                                    GTK_DIR_TAB_FORWARD);

        }

      gtk_cell_area_set_focus_cell (list_view->priv->cell_area, cell);
      return;
    }

  if (list_view->priv->modify_selection_pressed ||
      !list_view->priv->extend_selection_pressed ||
      !list_view->priv->anchor_item ||
      list_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    list_view->priv->anchor_item = item;

  cell = gtk_cell_area_get_focus_cell (list_view->priv->cell_area);
  _gtk_list_view_set_cursor_item (list_view, item, cell);

  if (!list_view->priv->modify_selection_pressed &&
      list_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = gtk_list_view_unselect_all_internal (list_view);
      dirty = gtk_list_view_select_all_between (list_view,
						list_view->priv->anchor_item,
						item) || dirty;
    }

  gtk_list_view_scroll_to_item (list_view, item);

  if (dirty)
    g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0);
}

static void
gtk_list_view_move_cursor_page_up_down (GtkListView *list_view,
					gint         count)
{
  GtkListViewItem *item;
  gboolean dirty = FALSE;
 
  if (!gtk_widget_has_focus (GTK_WIDGET (list_view)))
    return;
 
  if (!list_view->priv->cursor_item)
    {
      GList *list;

      if (count > 0)
	list = list_view->priv->items;
      else
	list = g_list_last (list_view->priv->items);

      item = list ? list->data : NULL;
    }
  else
    item = find_item_page_up_down (list_view,
				   list_view->priv->cursor_item,
				   count);

  if (item == list_view->priv->cursor_item)
    gtk_widget_error_bell (GTK_WIDGET (list_view));

  if (!item)
    return;

  if (list_view->priv->modify_selection_pressed ||
      !list_view->priv->extend_selection_pressed ||
      !list_view->priv->anchor_item ||
      list_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    list_view->priv->anchor_item = item;

  _gtk_list_view_set_cursor_item (list_view, item, NULL);

  if (!list_view->priv->modify_selection_pressed &&
      list_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = gtk_list_view_unselect_all_internal (list_view);
      dirty = gtk_list_view_select_all_between (list_view,
						list_view->priv->anchor_item,
						item) || dirty;
    }

  gtk_list_view_scroll_to_item (list_view, item);

  if (dirty)
    g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0); 
}

static void
gtk_list_view_move_cursor_left_right (GtkListView *list_view,
				      gint         count)
{
  GtkListViewItem *item;
  GtkCellRenderer *cell = NULL;
  gboolean dirty = FALSE;
  gint step;
  GtkDirectionType direction;

  if (!gtk_widget_has_focus (GTK_WIDGET (list_view)))
    return;

  direction = count < 0 ? GTK_DIR_LEFT : GTK_DIR_RIGHT;

  if (!list_view->priv->cursor_item)
    {
      GList *list;

      if (count > 0)
	list = list_view->priv->items;
      else
	list = g_list_last (list_view->priv->items);

      if (list)
        {
          item = list->data;

          /* Give focus to the first cell initially */
          _gtk_list_view_set_cell_data (list_view, item);
          gtk_cell_area_focus (list_view->priv->cell_area, direction);
        }
      else
        {
          item = NULL;
        }
    }
  else
    {
      item = list_view->priv->cursor_item;
      step = count > 0 ? 1 : -1;

      /* Save the current focus cell in case we hit the edge */
      cell = gtk_cell_area_get_focus_cell (list_view->priv->cell_area);

      while (item)
	{
	  _gtk_list_view_set_cell_data (list_view, item);

	  if (gtk_cell_area_focus (list_view->priv->cell_area, direction))
	    break;
	 
	  item = find_item (list_view, item, 0, step);
	}
    }

  if (!item)
    {
      if (!gtk_widget_keynav_failed (GTK_WIDGET (list_view), direction))
        {
          GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (list_view));
          if (toplevel)
            gtk_widget_child_focus (toplevel,
                                    direction == GTK_DIR_LEFT ?
                                    GTK_DIR_TAB_BACKWARD :
                                    GTK_DIR_TAB_FORWARD);

        }

      gtk_cell_area_set_focus_cell (list_view->priv->cell_area, cell);
      return;
    }

  if (list_view->priv->modify_selection_pressed ||
      !list_view->priv->extend_selection_pressed ||
      !list_view->priv->anchor_item ||
      list_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    list_view->priv->anchor_item = item;

  cell = gtk_cell_area_get_focus_cell (list_view->priv->cell_area);
  _gtk_list_view_set_cursor_item (list_view, item, cell);

  if (!list_view->priv->modify_selection_pressed &&
      list_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = gtk_list_view_unselect_all_internal (list_view);
      dirty = gtk_list_view_select_all_between (list_view,
						list_view->priv->anchor_item,
						item) || dirty;
    }

  gtk_list_view_scroll_to_item (list_view, item);

  if (dirty)
    g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0);
}

static void
gtk_list_view_move_cursor_start_end (GtkListView *list_view,
				     gint         count)
{
  GtkListViewItem *item;
  GList *list;
  gboolean dirty = FALSE;
 
  if (!gtk_widget_has_focus (GTK_WIDGET (list_view)))
    return;
 
  if (count < 0)
    list = list_view->priv->items;
  else
    list = g_list_last (list_view->priv->items);
 
  item = list ? list->data : NULL;

  if (item == list_view->priv->cursor_item)
    gtk_widget_error_bell (GTK_WIDGET (list_view));

  if (!item)
    return;

  if (list_view->priv->modify_selection_pressed ||
      !list_view->priv->extend_selection_pressed ||
      !list_view->priv->anchor_item ||
      list_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    list_view->priv->anchor_item = item;

  _gtk_list_view_set_cursor_item (list_view, item, NULL);

  if (!list_view->priv->modify_selection_pressed &&
      list_view->priv->selection_mode != GTK_SELECTION_NONE)
    {
      dirty = gtk_list_view_unselect_all_internal (list_view);
      dirty = gtk_list_view_select_all_between (list_view,
						list_view->priv->anchor_item,
						item) || dirty;
    }

  gtk_list_view_scroll_to_item (list_view, item);

  if (dirty)
    g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0);
}

/**
 * gtk_list_view_scroll_to_path:
 * @list_view: A #GtkListView.
 * @path: The path of the item to move to.
 * @use_align: whether to use alignment arguments, or %FALSE.
 * @row_align: The vertical alignment of the item specified by @path.
 * @col_align: The horizontal alignment of the item specified by @path.
 *
 * Moves the alignments of @list_view to the position specified by @path. 
 * @row_align determines where the row is placed, and @col_align determines
 * where @column is placed.  Both are expected to be between 0.0 and 1.0.
 * 0.0 means left/top alignment, 1.0 means right/bottom alignment, 0.5 means
 * center.
 *
 * If @use_align is %FALSE, then the alignment arguments are ignored, and the
 * tree does the minimum amount of work to scroll the item onto the screen.
 * This means that the item will be scrolled to the edge closest to its current
 * position.  If the item is currently visible on the screen, nothing is done.
 *
 * This function only works if the model is set, and @path is a valid row on
 * the model. If the model changes before the @list_view is realized, the
 * centered path will be modified to reflect this change.
 *
 * Since: 2.8
 **/
void
gtk_list_view_scroll_to_path (GtkListView *list_view,
			      GtkTreePath *path,
			      gboolean     use_align,
			      gfloat       row_align,
			      gfloat       col_align)
{
  GtkListViewItem *item = NULL;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (path != NULL);
  g_return_if_fail (row_align >= 0.0 && row_align <= 1.0);
  g_return_if_fail (col_align >= 0.0 && col_align <= 1.0);

  widget = GTK_WIDGET (list_view);

  if (gtk_tree_path_get_depth (path) > 0)
    item = g_list_nth_data (list_view->priv->items,
			    gtk_tree_path_get_indices(path)[0]);
 
  if (!item || item->cell_area.width < 0 ||
      !gtk_widget_get_realized (widget))
    {
      if (list_view->priv->scroll_to_path)
	gtk_tree_row_reference_free (list_view->priv->scroll_to_path);

      list_view->priv->scroll_to_path = NULL;

      if (path)
	list_view->priv->scroll_to_path = gtk_tree_row_reference_new_proxy (G_OBJECT (list_view), list_view->priv->model, path);

      list_view->priv->scroll_to_use_align = use_align;
      list_view->priv->scroll_to_row_align = row_align;
      list_view->priv->scroll_to_col_align = col_align;

      return;
    }

  if (use_align)
    {
      GtkAllocation allocation;
      gint x, y;
      gfloat offset;
      GdkRectangle item_area =
	{
	  item->cell_area.x - list_view->priv->item_padding,
	  item->cell_area.y - list_view->priv->item_padding,
	  item->cell_area.width  + list_view->priv->item_padding * 2,
	  item->cell_area.height + list_view->priv->item_padding * 2
	};

      gdk_window_get_position (list_view->priv->bin_window, &x, &y);

      gtk_widget_get_allocation (widget, &allocation);

      offset = y + item_area.y - row_align * (allocation.height - item_area.height);

      gtk_adjustment_set_value (list_view->priv->vadjustment,
                                gtk_adjustment_get_value (list_view->priv->vadjustment) + offset);

      offset = x + item_area.x - col_align * (allocation.width - item_area.width);

      gtk_adjustment_set_value (list_view->priv->hadjustment,
                                gtk_adjustment_get_value (list_view->priv->hadjustment) + offset);

      gtk_adjustment_changed (list_view->priv->hadjustment);
      gtk_adjustment_changed (list_view->priv->vadjustment);
    }
  else
    gtk_list_view_scroll_to_item (list_view, item);
}


static void
gtk_list_view_scroll_to_item (GtkListView     *list_view,
                              GtkListViewItem *item)
{
  GtkListViewPrivate *priv = list_view->priv;
  GtkWidget *widget = GTK_WIDGET (list_view);
  GtkAdjustment *hadj, *vadj;
  GtkAllocation allocation;
  gint x, y;
  GdkRectangle item_area;

  item_area.x = item->cell_area.x - priv->item_padding;
  item_area.y = item->cell_area.y - priv->item_padding;
  item_area.width = item->cell_area.width  + priv->item_padding * 2;
  item_area.height = item->cell_area.height + priv->item_padding * 2;

  gdk_window_get_position (list_view->priv->bin_window, &x, &y);
  gtk_widget_get_allocation (widget, &allocation);

  hadj = list_view->priv->hadjustment;
  vadj = list_view->priv->vadjustment;

  if (y + item_area.y < 0)
    gtk_adjustment_set_value (vadj,
                              gtk_adjustment_get_value (vadj)
                                + y + item_area.y);
  else if (y + item_area.y + item_area.height > allocation.height)
    gtk_adjustment_set_value (vadj,
                              gtk_adjustment_get_value (vadj)
                                + y + item_area.y + item_area.height - allocation.height);

  if (x + item_area.x < 0)
    gtk_adjustment_set_value (hadj,
                              gtk_adjustment_get_value (hadj)
                                + x + item_area.x);
  else if (x + item_area.x + item_area.width > allocation.width)
    gtk_adjustment_set_value (hadj,
                              gtk_adjustment_get_value (hadj)
                                + x + item_area.x + item_area.width - allocation.width);

  gtk_adjustment_changed (hadj);
  gtk_adjustment_changed (vadj);
}

/* GtkCellLayout implementation */

static void
gtk_list_view_ensure_cell_area (GtkListView *list_view,
                                GtkCellArea *cell_area)
{
  GtkListViewPrivate *priv = list_view->priv;

  if (priv->cell_area)
    return;

  if (cell_area)
    priv->cell_area = cell_area;
  else
    priv->cell_area = gtk_cell_area_box_new ();

  g_object_ref_sink (priv->cell_area);

  if (GTK_IS_ORIENTABLE (priv->cell_area))
    gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->cell_area), priv->item_orientation);

  priv->cell_area_context = gtk_cell_area_create_context (priv->cell_area);

  priv->add_editable_id =
    g_signal_connect (priv->cell_area, "add-editable",
                      G_CALLBACK (gtk_list_view_add_editable), list_view);
  priv->remove_editable_id =
    g_signal_connect (priv->cell_area, "remove-editable",
                      G_CALLBACK (gtk_list_view_remove_editable), list_view);

  update_text_cell (list_view);
  update_pixbuf_cell (list_view);
}

static GtkCellArea *
gtk_list_view_cell_layout_get_area (GtkCellLayout *cell_layout)
{
  GtkListView *list_view = GTK_LIST_VIEW (cell_layout);
  GtkListViewPrivate *priv = list_view->priv;

  if (G_UNLIKELY (!priv->cell_area))
    gtk_list_view_ensure_cell_area (list_view, NULL);

  return list_view->priv->cell_area;
}

void
_gtk_list_view_set_cell_data (GtkListView     *list_view,
			      GtkListViewItem *item)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  path = gtk_tree_path_new_from_indices (item->index, -1);
  if (!gtk_tree_model_get_iter (list_view->priv->model, &iter, path))
    return;
  gtk_tree_path_free (path);

  gtk_cell_area_apply_attributes (list_view->priv->cell_area,
				  list_view->priv->model,
				  &iter, FALSE, FALSE);
}



/* Public API */


/**
 * gtk_list_view_convert_widget_to_bin_window_coords:
 * @list_view: a #GtkListView
 * @wx: X coordinate relative to the widget
 * @wy: Y coordinate relative to the widget
 * @bx: (out): return location for bin_window X coordinate
 * @by: (out): return location for bin_window Y coordinate
 *
 * Converts widget coordinates to coordinates for the bin_window,
 * as expected by e.g. gtk_list_view_get_path_at_pos().
 *
 * Since: 2.12
 */
void
gtk_list_view_convert_widget_to_bin_window_coords (GtkListView *list_view,
                                                   gint         wx,
                                                   gint         wy,
                                                   gint        *bx,
                                                   gint        *by)
{
  gint x, y;

  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  if (list_view->priv->bin_window)
    gdk_window_get_position (list_view->priv->bin_window, &x, &y);
  else
    x = y = 0;

  if (bx)
    *bx = wx - x;
  if (by)
    *by = wy - y;
}

/**
 * gtk_list_view_get_path_at_pos:
 * @list_view: A #GtkListView.
 * @x: The x position to be identified
 * @y: The y position to be identified
 *
 * Finds the path at the point (@x, @y), relative to bin_window coordinates.
 * See gtk_list_view_get_item_at_pos(), if you are also interested in
 * the cell at the specified position.
 * See gtk_list_view_convert_widget_to_bin_window_coords() for converting
 * widget coordinates to bin_window coordinates.
 *
 * Return value: The #GtkTreePath corresponding to the list or %NULL
 * if no list exists at that position.
 *
 * Since: 2.6
 **/
GtkTreePath *
gtk_list_view_get_path_at_pos (GtkListView *list_view,
			       gint         x,
			       gint         y)
{
  GtkListViewItem *item;
  GtkTreePath *path;
 
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), NULL);

  item = _gtk_list_view_get_item_at_coords (list_view, x, y, TRUE, NULL);

  if (!item)
    return NULL;

  path = gtk_tree_path_new_from_indices (item->index, -1);

  return path;
}

/**
 * gtk_list_view_get_item_at_pos:
 * @list_view: A #GtkListView.
 * @x: The x position to be identified
 * @y: The y position to be identified
 * @path: (out) (allow-none): Return location for the path, or %NULL
 * @cell: (out) (allow-none): Return location for the renderer
 *   responsible for the cell at (@x, @y), or %NULL
 *
 * Finds the path at the point (@x, @y), relative to bin_window coordinates.
 * In contrast to gtk_list_view_get_path_at_pos(), this function also
 * obtains the cell at the specified position. The returned path should
 * be freed with gtk_tree_path_free().
 * See gtk_list_view_convert_widget_to_bin_window_coords() for converting
 * widget coordinates to bin_window coordinates.
 *
 * Return value: %TRUE if an item exists at the specified position
 *
 * Since: 2.8
 **/
gboolean
gtk_list_view_get_item_at_pos (GtkListView      *list_view,
			       gint              x,
			       gint              y,
			       GtkTreePath     **path,
			       GtkCellRenderer **cell)
{
  GtkListViewItem *item;
  GtkCellRenderer *renderer = NULL;
 
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), FALSE);

  item = _gtk_list_view_get_item_at_coords (list_view, x, y, TRUE, &renderer);

  if (path != NULL)
    {
      if (item != NULL)
	*path = gtk_tree_path_new_from_indices (item->index, -1);
      else
	*path = NULL;
    }

  if (cell != NULL)
    *cell = renderer;

  return (item != NULL);
}

/**
 * gtk_list_view_set_tooltip_item:
 * @list_view: a #GtkListView
 * @tooltip: a #GtkTooltip
 * @path: a #GtkTreePath
 *
 * Sets the tip area of @tooltip to be the area covered by the item at @path.
 * See also gtk_list_view_set_tooltip_column() for a simpler alternative.
 * See also gtk_tooltip_set_tip_area().
 *
 * Since: 2.12
 */
void
gtk_list_view_set_tooltip_item (GtkListView     *list_view,
                                GtkTooltip      *tooltip,
                                GtkTreePath     *path)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_list_view_set_tooltip_cell (list_view, tooltip, path, NULL);
}

/**
 * gtk_list_view_set_tooltip_cell:
 * @list_view: a #GtkListView
 * @tooltip: a #GtkTooltip
 * @path: a #GtkTreePath
 * @cell: (allow-none): a #GtkCellRenderer or %NULL
 *
 * Sets the tip area of @tooltip to the area which @cell occupies in
 * the item pointed to by @path. See also gtk_tooltip_set_tip_area().
 *
 * See also gtk_list_view_set_tooltip_column() for a simpler alternative.
 *
 * Since: 2.12
 */
void
gtk_list_view_set_tooltip_cell (GtkListView     *list_view,
                                GtkTooltip      *tooltip,
                                GtkTreePath     *path,
                                GtkCellRenderer *cell)
{
  GdkRectangle rect;
  GtkListViewItem *item = NULL;
  gint x, y;

  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  g_return_if_fail (cell == NULL || GTK_IS_CELL_RENDERER (cell));

  if (gtk_tree_path_get_depth (path) > 0)
    item = g_list_nth_data (list_view->priv->items,
                            gtk_tree_path_get_indices(path)[0]);

  if (!item)
    return;

  if (cell)
    {
      _gtk_list_view_set_cell_data (list_view, item);
      gtk_cell_area_context_allocate (list_view->priv->cell_area_context, item->cell_area.width, item->cell_area.height);
      gtk_cell_area_get_cell_allocation (list_view->priv->cell_area,
                                         list_view->priv->cell_area_context,
					 GTK_WIDGET (list_view),
					 cell, &item->cell_area, &rect);
    }
  else
    {
      rect.x = item->cell_area.x - list_view->priv->item_padding;
      rect.y = item->cell_area.y - list_view->priv->item_padding;
      rect.width  = item->cell_area.width  + list_view->priv->item_padding * 2;
      rect.height = item->cell_area.height + list_view->priv->item_padding * 2;
    }
 
  if (list_view->priv->bin_window)
    {
      gdk_window_get_position (list_view->priv->bin_window, &x, &y);
      rect.x += x;
      rect.y += y;
    }

  gtk_tooltip_set_tip_area (tooltip, &rect);
}


/**
 * gtk_list_view_get_tooltip_context:
 * @list_view: an #GtkListView
 * @x: (inout): the x coordinate (relative to widget coordinates)
 * @y: (inout): the y coordinate (relative to widget coordinates)
 * @keyboard_tip: whether this is a keyboard tooltip or not
 * @model: (out) (allow-none): a pointer to receive a #GtkTreeModel or %NULL
 * @path: (out) (allow-none): a pointer to receive a #GtkTreePath or %NULL
 * @iter: (out) (allow-none): a pointer to receive a #GtkTreeIter or %NULL
 *
 * This function is supposed to be used in a #GtkWidget::query-tooltip
 * signal handler for #GtkListView.  The @x, @y and @keyboard_tip values
 * which are received in the signal handler, should be passed to this
 * function without modification.
 *
 * The return value indicates whether there is an list view item at the given
 * coordinates (%TRUE) or not (%FALSE) for mouse tooltips. For keyboard
 * tooltips the item returned will be the cursor item. When %TRUE, then any of
 * @model, @path and @iter which have been provided will be set to point to
 * that row and the corresponding model. @x and @y will always be converted
 * to be relative to @list_view's bin_window if @keyboard_tooltip is %FALSE.
 *
 * Return value: whether or not the given tooltip context points to a item
 *
 * Since: 2.12
 */
gboolean
gtk_list_view_get_tooltip_context (GtkListView   *list_view,
                                   gint          *x,
                                   gint          *y,
                                   gboolean       keyboard_tip,
                                   GtkTreeModel **model,
                                   GtkTreePath  **path,
                                   GtkTreeIter   *iter)
{
  GtkTreePath *tmppath = NULL;

  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), FALSE);
  g_return_val_if_fail (x != NULL, FALSE);
  g_return_val_if_fail (y != NULL, FALSE);

  if (keyboard_tip)
    {
      gtk_list_view_get_cursor (list_view, &tmppath, NULL);

      if (!tmppath)
        return FALSE;
    }
  else
    {
      gtk_list_view_convert_widget_to_bin_window_coords (list_view, *x, *y,
                                                         x, y);

      if (!gtk_list_view_get_item_at_pos (list_view, *x, *y, &tmppath, NULL))
        return FALSE;
    }

  if (model)
    *model = gtk_list_view_get_model (list_view);

  if (iter)
    gtk_tree_model_get_iter (gtk_list_view_get_model (list_view),
                             iter, tmppath);

  if (path)
    *path = tmppath;
  else
    gtk_tree_path_free (tmppath);

  return TRUE;
}

static gboolean
gtk_list_view_set_tooltip_query_cb (GtkWidget  *widget,
                                    gint        x,
                                    gint        y,
                                    gboolean    keyboard_tip,
                                    GtkTooltip *tooltip,
                                    gpointer    data)
{
  gchar *str;
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkListView *list_view = GTK_LIST_VIEW (widget);

  if (!gtk_list_view_get_tooltip_context (GTK_LIST_VIEW (widget),
                                          &x, &y,
                                          keyboard_tip,
                                          &model, &path, &iter))
    return FALSE;

  gtk_tree_model_get (model, &iter, list_view->priv->tooltip_column, &str, -1);

  if (!str)
    {
      gtk_tree_path_free (path);
      return FALSE;
    }

  gtk_tooltip_set_markup (tooltip, str);
  gtk_list_view_set_tooltip_item (list_view, tooltip, path);

  gtk_tree_path_free (path);
  g_free (str);

  return TRUE;
}


/**
 * gtk_list_view_set_tooltip_column:
 * @list_view: a #GtkListView
 * @column: an integer, which is a valid column number for @list_view's model
 *
 * If you only plan to have simple (text-only) tooltips on full items, you
 * can use this function to have #GtkListView handle these automatically
 * for you. @column should be set to the column in @list_view's model
 * containing the tooltip texts, or -1 to disable this feature.
 *
 * When enabled, #GtkWidget:has-tooltip will be set to %TRUE and
 * @list_view will connect a #GtkWidget::query-tooltip signal handler.
 *
 * Note that the signal handler sets the text with gtk_tooltip_set_markup(),
 * so &amp;, &lt;, etc have to be escaped in the text.
 *
 * Since: 2.12
 */
void
gtk_list_view_set_tooltip_column (GtkListView *list_view,
                                  gint         column)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  if (column == list_view->priv->tooltip_column)
    return;

  if (column == -1)
    {
      g_signal_handlers_disconnect_by_func (list_view,
                                            gtk_list_view_set_tooltip_query_cb,
                                            NULL);
      gtk_widget_set_has_tooltip (GTK_WIDGET (list_view), FALSE);
    }
  else
    {
      if (list_view->priv->tooltip_column == -1)
        {
          g_signal_connect (list_view, "query-tooltip",
                            G_CALLBACK (gtk_list_view_set_tooltip_query_cb), NULL);
          gtk_widget_set_has_tooltip (GTK_WIDGET (list_view), TRUE);
        }
    }

  list_view->priv->tooltip_column = column;
  g_object_notify (G_OBJECT (list_view), "tooltip-column");
}

/**
 * gtk_list_view_get_tooltip_column:
 * @list_view: a #GtkListView
 *
 * Returns the column of @list_view's model which is being used for
 * displaying tooltips on @list_view's rows.
 *
 * Return value: the index of the tooltip column that is currently being
 * used, or -1 if this is disabled.
 *
 * Since: 2.12
 */
gint
gtk_list_view_get_tooltip_column (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), 0);

  return list_view->priv->tooltip_column;
}

/**
 * gtk_list_view_get_visible_range:
 * @list_view: A #GtkListView
 * @start_path: (out) (allow-none): Return location for start of region,
 *              or %NULL
 * @end_path: (out) (allow-none): Return location for end of region, or %NULL
 *
 * Sets @start_path and @end_path to be the first and last visible path.
 * Note that there may be invisible paths in between.
 *
 * Both paths should be freed with gtk_tree_path_free() after use.
 *
 * Return value: %TRUE, if valid paths were placed in @start_path and @end_path
 *
 * Since: 2.8
 **/
gboolean
gtk_list_view_get_visible_range (GtkListView  *list_view,
				 GtkTreePath **start_path,
				 GtkTreePath **end_path)
{
  gint start_index = -1;
  gint end_index = -1;
  GList *lists;

  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), FALSE);

  if (list_view->priv->hadjustment == NULL ||
      list_view->priv->vadjustment == NULL)
    return FALSE;

  if (start_path == NULL && end_path == NULL)
    return FALSE;
 
  for (lists = list_view->priv->items; lists; lists = lists->next)
    {
      GtkListViewItem *item = lists->data;
      GdkRectangle    *item_area = &item->cell_area;

      if ((item_area->x + item_area->width >= (int)gtk_adjustment_get_value (list_view->priv->hadjustment)) &&
	  (item_area->y + item_area->height >= (int)gtk_adjustment_get_value (list_view->priv->vadjustment)) &&
	  (item_area->x <=
	   (int) (gtk_adjustment_get_value (list_view->priv->hadjustment) +
		  gtk_adjustment_get_page_size (list_view->priv->hadjustment))) &&
	  (item_area->y <=
	   (int) (gtk_adjustment_get_value (list_view->priv->vadjustment) +
		  gtk_adjustment_get_page_size (list_view->priv->vadjustment))))
	{
	  if (start_index == -1)
	    start_index = item->index;
	  end_index = item->index;
	}
    }

  if (start_path && start_index != -1)
    *start_path = gtk_tree_path_new_from_indices (start_index, -1);
  if (end_path && end_index != -1)
    *end_path = gtk_tree_path_new_from_indices (end_index, -1);
 
  return start_index != -1;
}

/**
 * gtk_list_view_selected_foreach:
 * @list_view: A #GtkListView.
 * @func: (scope call): The function to call for each selected list.
 * @data: User data to pass to the function.
 *
 * Calls a function for each selected list. Note that the model or
 * selection cannot be modified from within this function.
 *
 * Since: 2.6
 **/
void
gtk_list_view_selected_foreach (GtkListView           *list_view,
				GtkListViewForeachFunc func,
				gpointer               data)
{
  GList *list;
 
  for (list = list_view->priv->items; list; list = list->next)
    {
      GtkListViewItem *item = list->data;
      GtkTreePath *path = gtk_tree_path_new_from_indices (item->index, -1);

      if (item->selected)
	(* func) (list_view, path, data);

      gtk_tree_path_free (path);
    }
}

static void
update_text_cell (GtkListView *list_view)
{
  if (!list_view->priv->cell_area)
    return;

  if (list_view->priv->text_column == -1 &&
      list_view->priv->markup_column == -1)
    {
      if (list_view->priv->text_cell != NULL)
	{
	  gtk_cell_area_remove (list_view->priv->cell_area,
				list_view->priv->text_cell);
	  list_view->priv->text_cell = NULL;
	}
    }
  else
    {
      if (list_view->priv->text_cell == NULL)
	{
	  list_view->priv->text_cell = gtk_cell_renderer_text_new ();

	  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (list_view), list_view->priv->text_cell, FALSE);
	}

      if (list_view->priv->markup_column != -1)
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (list_view),
					list_view->priv->text_cell,
					"markup", list_view->priv->markup_column,
					NULL);
      else
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (list_view),
					list_view->priv->text_cell,
					"text", list_view->priv->text_column,
					NULL);

      if (list_view->priv->item_orientation == GTK_ORIENTATION_VERTICAL)
	g_object_set (list_view->priv->text_cell,
                      "alignment", PANGO_ALIGN_CENTER,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "xalign", 0.5,
		      "yalign", 0.0,
		      NULL);
      else
	g_object_set (list_view->priv->text_cell,
                      "alignment", PANGO_ALIGN_LEFT,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "xalign", 0.0,
		      "yalign", 0.5,
		      NULL);
    }
}

static void
update_pixbuf_cell (GtkListView *list_view)
{
  if (!list_view->priv->cell_area)
    return;

  if (list_view->priv->pixbuf_column == -1)
    {
      if (list_view->priv->pixbuf_cell != NULL)
	{
	  gtk_cell_area_remove (list_view->priv->cell_area,
				list_view->priv->pixbuf_cell);

	  list_view->priv->pixbuf_cell = NULL;
	}
    }
  else
    {
      if (list_view->priv->pixbuf_cell == NULL)
	{
	  list_view->priv->pixbuf_cell = gtk_cell_renderer_pixbuf_new ();
	 
	  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (list_view), list_view->priv->pixbuf_cell, FALSE);
	}
     
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (list_view),
				      list_view->priv->pixbuf_cell,
				      "pixbuf", list_view->priv->pixbuf_column,
				      NULL);

      if (list_view->priv->item_orientation == GTK_ORIENTATION_VERTICAL)
	g_object_set (list_view->priv->pixbuf_cell,
		      "xalign", 0.5,
		      "yalign", 1.0,
		      NULL);
      else
	g_object_set (list_view->priv->pixbuf_cell,
		      "xalign", 0.0,
		      "yalign", 0.0,
		      NULL);
    }
}

/**
 * gtk_list_view_set_text_column:
 * @list_view: A #GtkListView.
 * @column: A column in the currently used model, or -1 to display no text
 *
 * Sets the column with text for @list_view to be @column. The text
 * column must be of type #G_TYPE_STRING.
 *
 * Since: 2.6
 **/
void
gtk_list_view_set_text_column (GtkListView *list_view,
			       gint          column)
{
  if (column == list_view->priv->text_column)
    return;
 
  if (column == -1)
    list_view->priv->text_column = -1;
  else
    {
      if (list_view->priv->model != NULL)
	{
	  GType column_type;
	 
	  column_type = gtk_tree_model_get_column_type (list_view->priv->model, column);

	  g_return_if_fail (column_type == G_TYPE_STRING);
	}
     
      list_view->priv->text_column = column;
    }

  if (list_view->priv->cell_area)
    gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

  update_text_cell (list_view);

  gtk_list_view_invalidate_sizes (list_view);
 
  g_object_notify (G_OBJECT (list_view), "text-column");
}

/**
 * gtk_list_view_get_text_column:
 * @list_view: A #GtkListView.
 *
 * Returns the column with text for @list_view.
 *
 * Returns: the text column, or -1 if it's unset.
 *
 * Since: 2.6
 */
gint
gtk_list_view_get_text_column (GtkListView  *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);

  return list_view->priv->text_column;
}

/**
 * gtk_list_view_set_markup_column:
 * @list_view: A #GtkListView.
 * @column: A column in the currently used model, or -1 to display no text
 *
 * Sets the column with markup information for @list_view to be
 * @column. The markup column must be of type #G_TYPE_STRING.
 * If the markup column is set to something, it overrides
 * the text column set by gtk_list_view_set_text_column().
 *
 * Since: 2.6
 **/
void
gtk_list_view_set_markup_column (GtkListView *list_view,
				 gint         column)
{
  if (column == list_view->priv->markup_column)
    return;
 
  if (column == -1)
    list_view->priv->markup_column = -1;
  else
    {
      if (list_view->priv->model != NULL)
	{
	  GType column_type;
	 
	  column_type = gtk_tree_model_get_column_type (list_view->priv->model, column);

	  g_return_if_fail (column_type == G_TYPE_STRING);
	}
     
      list_view->priv->markup_column = column;
    }

  if (list_view->priv->cell_area)
    gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

  update_text_cell (list_view);

  gtk_list_view_invalidate_sizes (list_view);
 
  g_object_notify (G_OBJECT (list_view), "markup-column");
}

/**
 * gtk_list_view_get_markup_column:
 * @list_view: A #GtkListView.
 *
 * Returns the column with markup text for @list_view.
 *
 * Returns: the markup column, or -1 if it's unset.
 *
 * Since: 2.6
 */
gint
gtk_list_view_get_markup_column (GtkListView  *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);

  return list_view->priv->markup_column;
}

/**
 * gtk_list_view_set_pixbuf_column:
 * @list_view: A #GtkListView.
 * @column: A column in the currently used model, or -1 to disable
 *
 * Sets the column with pixbufs for @list_view to be @column. The pixbuf
 * column must be of type #GDK_TYPE_PIXBUF
 *
 * Since: 2.6
 **/
void
gtk_list_view_set_pixbuf_column (GtkListView *list_view,
				 gint         column)
{
  if (column == list_view->priv->pixbuf_column)
    return;
 
  if (column == -1)
    list_view->priv->pixbuf_column = -1;
  else
    {
      if (list_view->priv->model != NULL)
	{
	  GType column_type;
	 
	  column_type = gtk_tree_model_get_column_type (list_view->priv->model, column);

	  g_return_if_fail (column_type == GDK_TYPE_PIXBUF);
	}
     
      list_view->priv->pixbuf_column = column;
    }

  if (list_view->priv->cell_area)
    gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

  update_pixbuf_cell (list_view);

  gtk_list_view_invalidate_sizes (list_view);
 
  g_object_notify (G_OBJECT (list_view), "pixbuf-column");
 
}

/**
 * gtk_list_view_get_pixbuf_column:
 * @list_view: A #GtkListView.
 *
 * Returns the column with pixbufs for @list_view.
 *
 * Returns: the pixbuf column, or -1 if it's unset.
 *
 * Since: 2.6
 */
gint
gtk_list_view_get_pixbuf_column (GtkListView  *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);

  return list_view->priv->pixbuf_column;
}

/**
 * gtk_list_view_select_path:
 * @list_view: A #GtkListView.
 * @path: The #GtkTreePath to be selected.
 *
 * Selects the row at @path.
 *
 * Since: 2.6
 **/
void
gtk_list_view_select_path (GtkListView *list_view,
			   GtkTreePath *path)
{
  GtkListViewItem *item = NULL;

  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (list_view->priv->model != NULL);
  g_return_if_fail (path != NULL);

  if (gtk_tree_path_get_depth (path) > 0)
    item = g_list_nth_data (list_view->priv->items,
			    gtk_tree_path_get_indices(path)[0]);

  if (item)
    _gtk_list_view_select_item (list_view, item);
}

/**
 * gtk_list_view_unselect_path:
 * @list_view: A #GtkListView.
 * @path: The #GtkTreePath to be unselected.
 *
 * Unselects the row at @path.
 *
 * Since: 2.6
 **/
void
gtk_list_view_unselect_path (GtkListView *list_view,
			     GtkTreePath *path)
{
  GtkListViewItem *item;
 
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (list_view->priv->model != NULL);
  g_return_if_fail (path != NULL);

  item = g_list_nth_data (list_view->priv->items,
			  gtk_tree_path_get_indices(path)[0]);

  if (!item)
    return;
 
  _gtk_list_view_unselect_item (list_view, item);
}

/**
 * gtk_list_view_get_selected_items:
 * @list_view: A #GtkListView.
 *
 * Creates a list of paths of all selected items. Additionally, if you are
 * planning on modifying the model after calling this function, you may
 * want to convert the returned list into a list of #GtkTreeRowReference<!-- -->s.
 * To do this, you can use gtk_tree_row_reference_new().
 *
 * To free the return value, use:
 * |[
 * g_list_free_full (list, (GDestroyNotify) gtk_tree_patch_free);
 * ]|
 *
 * Return value: (element-type GtkTreePath) (transfer full): A #GList containing a #GtkTreePath for each selected row.
 *
 * Since: 2.6
 **/
GList *
gtk_list_view_get_selected_items (GtkListView *list_view)
{
  GList *list;
  GList *selected = NULL;
 
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), NULL);
 
  for (list = list_view->priv->items; list != NULL; list = list->next)
    {
      GtkListViewItem *item = list->data;

      if (item->selected)
	{
	  GtkTreePath *path = gtk_tree_path_new_from_indices (item->index, -1);

	  selected = g_list_prepend (selected, path);
	}
    }

  return selected;
}

/**
 * gtk_list_view_select_all:
 * @list_view: A #GtkListView.
 *
 * Selects all the lists. @list_view must has its selection mode set
 * to #GTK_SELECTION_MULTIPLE.
 *
 * Since: 2.6
 **/
void
gtk_list_view_select_all (GtkListView *list_view)
{
  GList *items;
  gboolean dirty = FALSE;
 
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  if (list_view->priv->selection_mode != GTK_SELECTION_MULTIPLE)
    return;

  for (items = list_view->priv->items; items; items = items->next)
    {
      GtkListViewItem *item = items->data;
     
      if (!item->selected)
	{
	  dirty = TRUE;
	  item->selected = TRUE;
	  gtk_list_view_queue_draw_item (list_view, item);
	}
    }

  if (dirty)
    g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0);
}

/**
 * gtk_list_view_unselect_all:
 * @list_view: A #GtkListView.
 *
 * Unselects all the lists.
 *
 * Since: 2.6
 **/
void
gtk_list_view_unselect_all (GtkListView *list_view)
{
  gboolean dirty = FALSE;
 
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  if (list_view->priv->selection_mode == GTK_SELECTION_BROWSE)
    return;

  dirty = gtk_list_view_unselect_all_internal (list_view);

  if (dirty)
    g_signal_emit (list_view, list_view_signals[SELECTION_CHANGED], 0);
}

/**
 * gtk_list_view_path_is_selected:
 * @list_view: A #GtkListView.
 * @path: A #GtkTreePath to check selection on.
 *
 * Returns %TRUE if the list pointed to by @path is currently
 * selected. If @path does not point to a valid location, %FALSE is returned.
 *
 * Return value: %TRUE if @path is selected.
 *
 * Since: 2.6
 **/
gboolean
gtk_list_view_path_is_selected (GtkListView *list_view,
				GtkTreePath *path)
{
  GtkListViewItem *item;
 
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), FALSE);
  g_return_val_if_fail (list_view->priv->model != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
 
  item = g_list_nth_data (list_view->priv->items,
			  gtk_tree_path_get_indices(path)[0]);

  if (!item)
    return FALSE;
 
  return item->selected;
}

/**
 * gtk_list_view_get_item_row:
 * @list_view: a #GtkListView
 * @path: the #GtkTreePath of the item
 *
 * Gets the row in which the item @path is currently
 * displayed. Row numbers start at 0.
 *
 * Returns: The row in which the item is displayed
 *
 * Since: 2.22
 */
gint
gtk_list_view_get_item_row (GtkListView *list_view,
                            GtkTreePath *path)
{
  GtkListViewItem *item;

  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);
  g_return_val_if_fail (list_view->priv->model != NULL, -1);
  g_return_val_if_fail (path != NULL, -1);

  item = g_list_nth_data (list_view->priv->items,
                          gtk_tree_path_get_indices(path)[0]);

  if (!item)
    return -1;

  return item->row;
}

/**
 * gtk_list_view_get_item_column:
 * @list_view: a #GtkListView
 * @path: the #GtkTreePath of the item
 *
 * Gets the column in which the item @path is currently
 * displayed. Column numbers start at 0.
 *
 * Returns: The column in which the item is displayed
 *
 * Since: 2.22
 */
gint
gtk_list_view_get_item_column (GtkListView *list_view,
                               GtkTreePath *path)
{
  GtkListViewItem *item;

  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);
  g_return_val_if_fail (list_view->priv->model != NULL, -1);
  g_return_val_if_fail (path != NULL, -1);

  item = g_list_nth_data (list_view->priv->items,
                          gtk_tree_path_get_indices(path)[0]);

  if (!item)
    return -1;

  return item->col;
}

/**
 * gtk_list_view_item_activated:
 * @list_view: A #GtkListView
 * @path: The #GtkTreePath to be activated
 *
 * Activates the item determined by @path.
 *
 * Since: 2.6
 **/
void
gtk_list_view_item_activated (GtkListView      *list_view,
			      GtkTreePath      *path)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
  g_return_if_fail (path != NULL);
 
  g_signal_emit (list_view, list_view_signals[ITEM_ACTIVATED], 0, path);
}

/**
 * gtk_list_view_set_item_orientation:
 * @list_view: a #GtkListView
 * @orientation: the relative position of texts and lists
 *
 * Sets the ::item-orientation property which determines whether the labels
 * are drawn beside the lists instead of below.
 *
 * Since: 2.6
 **/
void
gtk_list_view_set_item_orientation (GtkListView    *list_view,
                                    GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  if (list_view->priv->item_orientation != orientation)
    {
      list_view->priv->item_orientation = orientation;

      if (list_view->priv->cell_area)
	{
	  if (GTK_IS_ORIENTABLE (list_view->priv->cell_area))
	    gtk_orientable_set_orientation (GTK_ORIENTABLE (list_view->priv->cell_area),
					    list_view->priv->item_orientation);

	  gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);
	}

      gtk_list_view_invalidate_sizes (list_view);

      update_text_cell (list_view);
      update_pixbuf_cell (list_view);
     
      g_object_notify (G_OBJECT (list_view), "item-orientation");
    }
}

/**
 * gtk_list_view_get_item_orientation:
 * @list_view: a #GtkListView
 *
 * Returns the value of the ::item-orientation property which determines
 * whether the labels are drawn beside the lists instead of below.
 *
 * Return value: the relative position of texts and lists
 *
 * Since: 2.6
 **/
GtkOrientation
gtk_list_view_get_item_orientation (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view),
			GTK_ORIENTATION_VERTICAL);

  return list_view->priv->item_orientation;
}

/**
 * gtk_list_view_set_columns:
 * @list_view: a #GtkListView
 * @columns: the number of columns
 *
 * Sets the ::columns property which determines in how
 * many columns the lists are arranged. If @columns is
 * -1, the number of columns will be chosen automatically
 * to fill the available area.
 *
 * Since: 2.6
 */
void
gtk_list_view_set_columns (GtkListView *list_view,
			   gint         columns)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
 
  if (list_view->priv->columns != columns)
    {
      list_view->priv->columns = columns;

      if (list_view->priv->cell_area)
	gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

      gtk_widget_queue_resize (GTK_WIDGET (list_view));
     
      g_object_notify (G_OBJECT (list_view), "columns");
    } 
}

/**
 * gtk_list_view_get_columns:
 * @list_view: a #GtkListView
 *
 * Returns the value of the ::columns property.
 *
 * Return value: the number of columns, or -1
 *
 * Since: 2.6
 */
gint
gtk_list_view_get_columns (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);

  return list_view->priv->columns;
}

/**
 * gtk_list_view_set_item_width:
 * @list_view: a #GtkListView
 * @item_width: the width for each item
 *
 * Sets the ::item-width property which specifies the width
 * to use for each item. If it is set to -1, the list view will
 * automatically determine a suitable item size.
 *
 * Since: 2.6
 */
void
gtk_list_view_set_item_width (GtkListView *list_view,
			      gint         item_width)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
 
  if (list_view->priv->item_width != item_width)
    {
      list_view->priv->item_width = item_width;
     
      if (list_view->priv->cell_area)
	gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

      gtk_list_view_invalidate_sizes (list_view);
     
      update_text_cell (list_view);

      g_object_notify (G_OBJECT (list_view), "item-width");
    } 
}

/**
 * gtk_list_view_get_item_width:
 * @list_view: a #GtkListView
 *
 * Returns the value of the ::item-width property.
 *
 * Return value: the width of a single item, or -1
 *
 * Since: 2.6
 */
gint
gtk_list_view_get_item_width (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);

  return list_view->priv->item_width;
}


/**
 * gtk_list_view_set_spacing:
 * @list_view: a #GtkListView
 * @spacing: the spacing
 *
 * Sets the ::spacing property which specifies the space
 * which is inserted between the cells (i.e. the list and
 * the text) of an item.
 *
 * Since: 2.6
 */
void
gtk_list_view_set_spacing (GtkListView *list_view,
			   gint         spacing)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
 
  if (list_view->priv->spacing != spacing)
    {
      list_view->priv->spacing = spacing;

      if (list_view->priv->cell_area)
	gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

      gtk_list_view_invalidate_sizes (list_view);

      g_object_notify (G_OBJECT (list_view), "spacing");
    } 
}

/**
 * gtk_list_view_get_spacing:
 * @list_view: a #GtkListView
 *
 * Returns the value of the ::spacing property.
 *
 * Return value: the space between cells
 *
 * Since: 2.6
 */
gint
gtk_list_view_get_spacing (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);

  return list_view->priv->spacing;
}

/**
 * gtk_list_view_set_row_spacing:
 * @list_view: a #GtkListView
 * @row_spacing: the row spacing
 *
 * Sets the ::row-spacing property which specifies the space
 * which is inserted between the rows of the list view.
 *
 * Since: 2.6
 */
void
gtk_list_view_set_row_spacing (GtkListView *list_view,
			       gint         row_spacing)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
 
  if (list_view->priv->row_spacing != row_spacing)
    {
      list_view->priv->row_spacing = row_spacing;

      if (list_view->priv->cell_area)
	gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

      gtk_list_view_invalidate_sizes (list_view);

      g_object_notify (G_OBJECT (list_view), "row-spacing");
    } 
}

/**
 * gtk_list_view_get_row_spacing:
 * @list_view: a #GtkListView
 *
 * Returns the value of the ::row-spacing property.
 *
 * Return value: the space between rows
 *
 * Since: 2.6
 */
gint
gtk_list_view_get_row_spacing (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);

  return list_view->priv->row_spacing;
}

/**
 * gtk_list_view_set_column_spacing:
 * @list_view: a #GtkListView
 * @column_spacing: the column spacing
 *
 * Sets the ::column-spacing property which specifies the space
 * which is inserted between the columns of the list view.
 *
 * Since: 2.6
 */
void
gtk_list_view_set_column_spacing (GtkListView *list_view,
				  gint         column_spacing)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
 
  if (list_view->priv->column_spacing != column_spacing)
    {
      list_view->priv->column_spacing = column_spacing;

      if (list_view->priv->cell_area)
	gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

      gtk_list_view_invalidate_sizes (list_view);

      g_object_notify (G_OBJECT (list_view), "column-spacing");
    } 
}

/**
 * gtk_list_view_get_column_spacing:
 * @list_view: a #GtkListView
 *
 * Returns the value of the ::column-spacing property.
 *
 * Return value: the space between columns
 *
 * Since: 2.6
 */
gint
gtk_list_view_get_column_spacing (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);

  return list_view->priv->column_spacing;
}

/**
 * gtk_list_view_set_margin:
 * @list_view: a #GtkListView
 * @margin: the margin
 *
 * Sets the ::margin property which specifies the space
 * which is inserted at the top, bottom, left and right
 * of the list view.
 *
 * Since: 2.6
 */
void
gtk_list_view_set_margin (GtkListView *list_view,
			  gint         margin)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
 
  if (list_view->priv->margin != margin)
    {
      list_view->priv->margin = margin;

      if (list_view->priv->cell_area)
	gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

      gtk_list_view_invalidate_sizes (list_view);

      g_object_notify (G_OBJECT (list_view), "margin");
    } 
}

/**
 * gtk_list_view_get_margin:
 * @list_view: a #GtkListView
 *
 * Returns the value of the ::margin property.
 *
 * Return value: the space at the borders
 *
 * Since: 2.6
 */
gint
gtk_list_view_get_margin (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);

  return list_view->priv->margin;
}

/**
 * gtk_list_view_set_item_padding:
 * @list_view: a #GtkListView
 * @item_padding: the item padding
 *
 * Sets the #GtkListView:item-padding property which specifies the padding
 * around each of the list view's items.
 *
 * Since: 2.18
 */
void
gtk_list_view_set_item_padding (GtkListView *list_view,
				gint         item_padding)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));
 
  if (list_view->priv->item_padding != item_padding)
    {
      list_view->priv->item_padding = item_padding;

      if (list_view->priv->cell_area)
	gtk_cell_area_stop_editing (list_view->priv->cell_area, TRUE);

      gtk_list_view_invalidate_sizes (list_view);

      g_object_notify (G_OBJECT (list_view), "item-padding");
    } 
}

/**
 * gtk_list_view_get_item_padding:
 * @list_view: a #GtkListView
 *
 * Returns the value of the ::item-padding property.
 *
 * Return value: the padding around items
 *
 * Since: 2.18
 */
gint
gtk_list_view_get_item_padding (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), -1);

  return list_view->priv->item_padding;
}

/* Get/set whether drag_motion requested the drag data and
 * drag_data_received should thus not actually insert the data,
 * since the data doesn't result from a drop.
 */
static void
set_status_pending (GdkDragContext *context,
                    GdkDragAction   suggested_action)
{
  g_object_set_data (G_OBJECT (context),
                     I_("gtk-list-view-status-pending"),
                     GINT_TO_POINTER (suggested_action));
}

static GdkDragAction
get_status_pending (GdkDragContext *context)
{
  return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (context),
                                             "gtk-list-view-status-pending"));
}

static void
unset_reorderable (GtkListView *list_view)
{
  if (list_view->priv->reorderable)
    {
      list_view->priv->reorderable = FALSE;
      g_object_notify (G_OBJECT (list_view), "reorderable");
    }
}

static void
set_source_row (GdkDragContext *context,
                GtkTreeModel   *model,
                GtkTreePath    *source_row)
{
  if (source_row)
    g_object_set_data_full (G_OBJECT (context),
			    I_("gtk-list-view-source-row"),
			    gtk_tree_row_reference_new (model, source_row),
			    (GDestroyNotify) gtk_tree_row_reference_free);
  else
    g_object_set_data_full (G_OBJECT (context),
			    I_("gtk-list-view-source-row"),
			    NULL, NULL);
}

static GtkTreePath*
get_source_row (GdkDragContext *context)
{
  GtkTreeRowReference *ref;

  ref = g_object_get_data (G_OBJECT (context), "gtk-list-view-source-row");

  if (ref)
    return gtk_tree_row_reference_get_path (ref);
  else
    return NULL;
}

typedef struct
{
  GtkTreeRowReference *dest_row;
  gboolean             empty_view_drop;
  gboolean             drop_append_mode;
} DestRow;

static void
dest_row_free (gpointer data)
{
  DestRow *dr = (DestRow *)data;

  gtk_tree_row_reference_free (dr->dest_row);
  g_free (dr);
}

static void
set_dest_row (GdkDragContext *context,
	      GtkTreeModel   *model,
	      GtkTreePath    *dest_row,
	      gboolean        empty_view_drop,
	      gboolean        drop_append_mode)
{
  DestRow *dr;

  if (!dest_row)
    {
      g_object_set_data_full (G_OBJECT (context),
			      I_("gtk-list-view-dest-row"),
			      NULL, NULL);
      return;
    }
 
  dr = g_new0 (DestRow, 1);
    
  dr->dest_row = gtk_tree_row_reference_new (model, dest_row);
  dr->empty_view_drop = empty_view_drop;
  dr->drop_append_mode = drop_append_mode;
  g_object_set_data_full (G_OBJECT (context),
                          I_("gtk-list-view-dest-row"),
                          dr, (GDestroyNotify) dest_row_free);
}

static GtkTreePath*
get_dest_row (GdkDragContext *context)
{
  DestRow *dr;

  dr = g_object_get_data (G_OBJECT (context), "gtk-list-view-dest-row");

  if (dr)
    {
      GtkTreePath *path = NULL;
     
      if (dr->dest_row)
	path = gtk_tree_row_reference_get_path (dr->dest_row);
      else if (dr->empty_view_drop)
	path = gtk_tree_path_new_from_indices (0, -1);
      else
	path = NULL;

      if (path && dr->drop_append_mode)
	gtk_tree_path_next (path);

      return path;
    }
  else
    return NULL;
}

static gboolean
check_model_dnd (GtkTreeModel *model,
                 GType         required_iface,
                 const gchar  *signal)
{
  if (model == NULL || !G_TYPE_CHECK_INSTANCE_TYPE ((model), required_iface))
    {
      g_warning ("You must override the default '%s' handler "
                 "on GtkListView when using models that don't support "
                 "the %s interface and enabling drag-and-drop. The simplest way to do this "
                 "is to connect to '%s' and call "
                 "g_signal_stop_emission_by_name() in your signal handler to prevent "
                 "the default handler from running. Look at the source code "
                 "for the default handler in gtklistview.c to get an idea what "
                 "your handler should do. (gtklistview.c is in the GTK+ source "
                 "code.) If you're using GTK+ from a language other than C, "
                 "there may be a more natural way to override default handlers, e.g. via derivation.",
                 signal, g_type_name (required_iface), signal);
      return FALSE;
    }
  else
    return TRUE;
}

static void
remove_scroll_timeout (GtkListView *list_view)
{
  if (list_view->priv->scroll_timeout_id != 0)
    {
      g_source_remove (list_view->priv->scroll_timeout_id);

      list_view->priv->scroll_timeout_id = 0;
    }
}

static void
gtk_list_view_autoscroll (GtkListView *list_view,
                          GdkDevice   *device)
{
  GdkWindow *window;
  gint px, py, width, height;
  gint hoffset, voffset;

  window = gtk_widget_get_window (GTK_WIDGET (list_view));

  gdk_window_get_device_position (window, device, &px, &py, NULL);
  gdk_window_get_geometry (window, NULL, NULL, &width, &height);

  /* see if we are near the edge. */
  voffset = py - 2 * SCROLL_EDGE_SIZE;
  if (voffset > 0)
    voffset = MAX (py - (height - 2 * SCROLL_EDGE_SIZE), 0);

  hoffset = px - 2 * SCROLL_EDGE_SIZE;
  if (hoffset > 0)
    hoffset = MAX (px - (width - 2 * SCROLL_EDGE_SIZE), 0);

  if (voffset != 0)
    gtk_adjustment_set_value (list_view->priv->vadjustment,
                              gtk_adjustment_get_value (list_view->priv->vadjustment) + voffset);

  if (hoffset != 0)
    gtk_adjustment_set_value (list_view->priv->hadjustment,
                              gtk_adjustment_get_value (list_view->priv->hadjustment) + hoffset);
}

typedef struct {
  GtkListView *list_view;
  GdkDevice   *device;
} DragScrollData;

static gboolean
drag_scroll_timeout (gpointer datap)
{
  DragScrollData *data = datap;

  gtk_list_view_autoscroll (data->list_view, data->device);

  return TRUE;
}

static void
drag_scroll_data_free (DragScrollData *data)
{
  g_slice_free (DragScrollData, data);
}

static gboolean
set_destination (GtkListView    *list_view,
		 GdkDragContext *context,
		 gint            x,
		 gint            y,
		 GdkDragAction  *suggested_action,
		 GdkAtom        *target)
{
  GtkWidget *widget;
  GtkTreePath *path = NULL;
  GtkListViewDropPosition pos;
  GtkListViewDropPosition old_pos;
  GtkTreePath *old_dest_path = NULL;
  gboolean can_drop = FALSE;

  widget = GTK_WIDGET (list_view);

  *suggested_action = 0;
  *target = GDK_NONE;

  if (!list_view->priv->dest_set)
    {
      /* someone unset us as a drag dest, note that if
       * we return FALSE drag_leave isn't called
       */

      gtk_list_view_set_drag_dest_item (list_view,
					NULL,
					GTK_LIST_VIEW_DROP_LEFT);

      remove_scroll_timeout (GTK_LIST_VIEW (widget));

      return FALSE; /* no longer a drop site */
    }

  *target = gtk_drag_dest_find_target (widget, context,
                                       gtk_drag_dest_get_target_list (widget));
  if (*target == GDK_NONE)
    return FALSE;

  if (!gtk_list_view_get_dest_item_at_pos (list_view, x, y, &path, &pos))
    {
      gint n_children;
      GtkTreeModel *model;
     
      /* the row got dropped on empty space, let's setup a special case
       */

      if (path)
	gtk_tree_path_free (path);

      model = gtk_list_view_get_model (list_view);

      n_children = gtk_tree_model_iter_n_children (model, NULL);
      if (n_children)
        {
          pos = GTK_LIST_VIEW_DROP_BELOW;
          path = gtk_tree_path_new_from_indices (n_children - 1, -1);
        }
      else
        {
          pos = GTK_LIST_VIEW_DROP_ABOVE;
          path = gtk_tree_path_new_from_indices (0, -1);
        }

      can_drop = TRUE;

      goto out;
    }

  g_assert (path);

  gtk_list_view_get_drag_dest_item (list_view,
				    &old_dest_path,
				    &old_pos);
 
  if (old_dest_path)
    gtk_tree_path_free (old_dest_path);
 
  if (TRUE /* FIXME if the location droppable predicate */)
    {
      can_drop = TRUE;
    }

out:
  if (can_drop)
    {
      GtkWidget *source_widget;

      *suggested_action = gdk_drag_context_get_suggested_action (context);
      source_widget = gtk_drag_get_source_widget (context);

      if (source_widget == widget)
        {
          /* Default to MOVE, unless the user has
           * pressed ctrl or shift to affect available actions
           */
          if ((gdk_drag_context_get_actions (context) & GDK_ACTION_MOVE) != 0)
            *suggested_action = GDK_ACTION_MOVE;
        }

      gtk_list_view_set_drag_dest_item (GTK_LIST_VIEW (widget),
					path, pos);
    }
  else
    {
      /* can't drop here */
      gtk_list_view_set_drag_dest_item (GTK_LIST_VIEW (widget),
					NULL,
					GTK_LIST_VIEW_DROP_LEFT);
    }
 
  if (path)
    gtk_tree_path_free (path);
 
  return TRUE;
}

static GtkTreePath*
get_logical_destination (GtkListView *list_view,
			 gboolean    *drop_append_mode)
{
  /* adjust path to point to the row the drop goes in front of */
  GtkTreePath *path = NULL;
  GtkListViewDropPosition pos;
 
  *drop_append_mode = FALSE;

  gtk_list_view_get_drag_dest_item (list_view, &path, &pos);

  if (path == NULL)
    return NULL;

  if (pos == GTK_LIST_VIEW_DROP_RIGHT ||
      pos == GTK_LIST_VIEW_DROP_BELOW)
    {
      GtkTreeIter iter;
      GtkTreeModel *model = list_view->priv->model;

      if (!gtk_tree_model_get_iter (model, &iter, path) ||
          !gtk_tree_model_iter_next (model, &iter))
        *drop_append_mode = TRUE;
      else
        {
          *drop_append_mode = FALSE;
          gtk_tree_path_next (path);
        }     
    }

  return path;
}

static gboolean
gtk_list_view_maybe_begin_drag (GtkListView    *list_view,
				GdkEventMotion *event)
{
  GtkWidget *widget = GTK_WIDGET (list_view);
  GdkDragContext *context;
  GtkTreePath *path = NULL;
  gint button;
  GtkTreeModel *model;
  gboolean retval = FALSE;

  if (!list_view->priv->source_set)
    goto out;

  if (list_view->priv->pressed_button < 0)
    goto out;

  if (!gtk_drag_check_threshold (GTK_WIDGET (list_view),
                                 list_view->priv->press_start_x,
                                 list_view->priv->press_start_y,
                                 event->x, event->y))
    goto out;

  model = gtk_list_view_get_model (list_view);

  if (model == NULL)
    goto out;

  button = list_view->priv->pressed_button;
  list_view->priv->pressed_button = -1;

  path = gtk_list_view_get_path_at_pos (list_view,
					list_view->priv->press_start_x,
					list_view->priv->press_start_y);

  if (path == NULL)
    goto out;

  if (!GTK_IS_TREE_DRAG_SOURCE (model) ||
      !gtk_tree_drag_source_row_draggable (GTK_TREE_DRAG_SOURCE (model),
					   path))
    goto out;

  /* FIXME Check whether we're a start button, if not return FALSE and
   * free path
   */

  /* Now we can begin the drag */
 
  retval = TRUE;

  context = gtk_drag_begin (widget,
                            gtk_drag_source_get_target_list (widget),
                            list_view->priv->source_actions,
                            button,
                            (GdkEvent*)event);

  set_source_row (context, model, path);
 
 out:
  if (path)
    gtk_tree_path_free (path);

  return retval;
}

/* Source side drag signals */
static void
gtk_list_view_drag_begin (GtkWidget      *widget,
			  GdkDragContext *context)
{
  GtkListView *list_view;
  GtkListViewItem *item;
  cairo_surface_t *list;
  gint x, y;
  GtkTreePath *path;

  list_view = GTK_LIST_VIEW (widget);

  /* if the user uses a custom DnD impl, we don't set the list here */
  if (!list_view->priv->dest_set && !list_view->priv->source_set)
    return;

  item = _gtk_list_view_get_item_at_coords (list_view,
					   list_view->priv->press_start_x,
					   list_view->priv->press_start_y,
					   TRUE,
					   NULL);

  g_return_if_fail (item != NULL);

  x = list_view->priv->press_start_x - item->cell_area.x + 1;
  y = list_view->priv->press_start_y - item->cell_area.y + 1;
 
  path = gtk_tree_path_new_from_indices (item->index, -1);
  list = gtk_list_view_create_drag_list (list_view, path);
  gtk_tree_path_free (path);

  cairo_surface_set_device_offset (list, -x, -y);

  gtk_drag_set_list_surface (context, list);

  cairo_surface_destroy (list);
}

static void
gtk_list_view_drag_end (GtkWidget      *widget,
			GdkDragContext *context)
{
  /* do nothing */
}

static void
gtk_list_view_drag_data_get (GtkWidget        *widget,
			     GdkDragContext   *context,
			     GtkSelectionData *selection_data,
			     guint             info,
			     guint             time)
{
  GtkListView *list_view;
  GtkTreeModel *model;
  GtkTreePath *source_row;

  list_view = GTK_LIST_VIEW (widget);
  model = gtk_list_view_get_model (list_view);

  if (model == NULL)
    return;

  if (!list_view->priv->source_set)
    return;

  source_row = get_source_row (context);

  if (source_row == NULL)
    return;

  /* We can implement the GTK_TREE_MODEL_ROW target generically for
   * any model; for DragSource models there are some other targets
   * we also support.
   */

  if (GTK_IS_TREE_DRAG_SOURCE (model) &&
      gtk_tree_drag_source_drag_data_get (GTK_TREE_DRAG_SOURCE (model),
                                          source_row,
                                          selection_data))
    goto done;

  /* If drag_data_get does nothing, try providing row data. */
  if (gtk_selection_data_get_target (selection_data) == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
    gtk_tree_set_row_drag_data (selection_data,
				model,
				source_row);

 done:
  gtk_tree_path_free (source_row);
}

static void
gtk_list_view_drag_data_delete (GtkWidget      *widget,
				GdkDragContext *context)
{
  GtkTreeModel *model;
  GtkListView *list_view;
  GtkTreePath *source_row;

  list_view = GTK_LIST_VIEW (widget);
  model = gtk_list_view_get_model (list_view);

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_SOURCE, "drag-data-delete"))
    return;

  if (!list_view->priv->source_set)
    return;

  source_row = get_source_row (context);

  if (source_row == NULL)
    return;

  gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (model),
                                         source_row);

  gtk_tree_path_free (source_row);

  set_source_row (context, NULL, NULL);
}

/* Target side drag signals */
static void
gtk_list_view_drag_leave (GtkWidget      *widget,
			  GdkDragContext *context,
			  guint           time)
{
  GtkListView *list_view;

  list_view = GTK_LIST_VIEW (widget);

  /* unset any highlight row */
  gtk_list_view_set_drag_dest_item (list_view,
				    NULL,
				    GTK_LIST_VIEW_DROP_LEFT);

  remove_scroll_timeout (list_view);
}

static gboolean
gtk_list_view_drag_motion (GtkWidget      *widget,
			   GdkDragContext *context,
			   gint            x,
			   gint            y,
			   guint           time)
{
  GtkTreePath *path = NULL;
  GtkListViewDropPosition pos;
  GtkListView *list_view;
  GdkDragAction suggested_action = 0;
  GdkAtom target;
  gboolean empty;

  list_view = GTK_LIST_VIEW (widget);

  if (!set_destination (list_view, context, x, y, &suggested_action, &target))
    return FALSE;

  gtk_list_view_get_drag_dest_item (list_view, &path, &pos);

  /* we only know this *after* set_desination_row */
  empty = list_view->priv->empty_view_drop;

  if (path == NULL && !empty)
    {
      /* Can't drop here. */
      gdk_drag_status (context, 0, time);
    }
  else
    {
      if (list_view->priv->scroll_timeout_id == 0)
	{
          DragScrollData *data = g_slice_new (DragScrollData);
          data->list_view = list_view;
          data->device = gdk_drag_context_get_device (context);

	  list_view->priv->scroll_timeout_id =
	    gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT, 50, drag_scroll_timeout, data, (GDestroyNotify) drag_scroll_data_free);
	}

      if (target == gdk_atom_intern_static_string ("GTK_TREE_MODEL_ROW"))
        {
          /* Request data so we can use the source row when
           * determining whether to accept the drop
           */
          set_status_pending (context, suggested_action);
          gtk_drag_get_data (widget, context, target, time);
        }
      else
        {
          set_status_pending (context, 0);
          gdk_drag_status (context, suggested_action, time);
        }
    }

  if (path)
    gtk_tree_path_free (path);

  return TRUE;
}

static gboolean
gtk_list_view_drag_drop (GtkWidget      *widget,
			 GdkDragContext *context,
			 gint            x,
			 gint            y,
			 guint           time)
{
  GtkListView *list_view;
  GtkTreePath *path;
  GdkDragAction suggested_action = 0;
  GdkAtom target = GDK_NONE;
  GtkTreeModel *model;
  gboolean drop_append_mode;

  list_view = GTK_LIST_VIEW (widget);
  model = gtk_list_view_get_model (list_view);

  remove_scroll_timeout (GTK_LIST_VIEW (widget));

  if (!list_view->priv->dest_set)
    return FALSE;

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag-drop"))
    return FALSE;

  if (!set_destination (list_view, context, x, y, &suggested_action, &target))
    return FALSE;
 
  path = get_logical_destination (list_view, &drop_append_mode);

  if (target != GDK_NONE && path != NULL)
    {
      /* in case a motion had requested drag data, change things so we
       * treat drag data receives as a drop.
       */
      set_status_pending (context, 0);
      set_dest_row (context, model, path,
		    list_view->priv->empty_view_drop, drop_append_mode);
    }

  if (path)
    gtk_tree_path_free (path);

  /* Unset this thing */
  gtk_list_view_set_drag_dest_item (list_view, NULL, GTK_LIST_VIEW_DROP_LEFT);

  if (target != GDK_NONE)
    {
      gtk_drag_get_data (widget, context, target, time);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_list_view_drag_data_received (GtkWidget        *widget,
				  GdkDragContext   *context,
				  gint              x,
				  gint              y,
				  GtkSelectionData *selection_data,
				  guint             info,
				  guint             time)
{
  GtkTreePath *path;
  gboolean accepted = FALSE;
  GtkTreeModel *model;
  GtkListView *list_view;
  GtkTreePath *dest_row;
  GdkDragAction suggested_action;
  gboolean drop_append_mode;
 
  list_view = GTK_LIST_VIEW (widget); 
  model = gtk_list_view_get_model (list_view);

  if (!check_model_dnd (model, GTK_TYPE_TREE_DRAG_DEST, "drag-data-received"))
    return;

  if (!list_view->priv->dest_set)
    return;

  suggested_action = get_status_pending (context);

  if (suggested_action)
    {
      /* We are getting this data due to a request in drag_motion,
       * rather than due to a request in drag_drop, so we are just
       * supposed to call drag_status, not actually paste in the
       * data.
       */
      path = get_logical_destination (list_view, &drop_append_mode);

      if (path == NULL)
        suggested_action = 0;

      if (suggested_action)
        {
	  if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
						     path,
						     selection_data))
	    suggested_action = 0;
        }

      gdk_drag_status (context, suggested_action, time);

      if (path)
        gtk_tree_path_free (path);

      /* If you can't drop, remove user drop indicator until the next motion */
      if (suggested_action == 0)
        gtk_list_view_set_drag_dest_item (list_view,
					  NULL,
					  GTK_LIST_VIEW_DROP_LEFT);
      return;
    }
 

  dest_row = get_dest_row (context);

  if (dest_row == NULL)
    return;

  if (gtk_selection_data_get_length (selection_data) >= 0)
    {
      if (gtk_tree_drag_dest_drag_data_received (GTK_TREE_DRAG_DEST (model),
                                                 dest_row,
                                                 selection_data))
        accepted = TRUE;
    }

  gtk_drag_finish (context,
                   accepted,
                   (gdk_drag_context_get_selected_action (context) == GDK_ACTION_MOVE),
                   time);

  gtk_tree_path_free (dest_row);

  /* drop dest_row */
  set_dest_row (context, NULL, NULL, FALSE, FALSE);
}

/* Drag-and-Drop support */
/**
 * gtk_list_view_enable_model_drag_source:
 * @list_view: a #GtkListTreeView
 * @start_button_mask: Mask of allowed buttons to start drag
 * @targets: (array length=n_targets): the table of targets that the drag will
 *           support
 * @n_targets: the number of items in @targets
 * @actions: the bitmask of possible actions for a drag from this
 *    widget
 *
 * Turns @list_view into a drag source for automatic DND. Calling this
 * method sets #GtkListView:reorderable to %FALSE.
 *
 * Since: 2.8
 **/
void
gtk_list_view_enable_model_drag_source (GtkListView              *list_view,
					GdkModifierType           start_button_mask,
					const GtkTargetEntry     *targets,
					gint                      n_targets,
					GdkDragAction             actions)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  gtk_drag_source_set (GTK_WIDGET (list_view), 0, targets, n_targets, actions);

  list_view->priv->start_button_mask = start_button_mask;
  list_view->priv->source_actions = actions;

  list_view->priv->source_set = TRUE;

  unset_reorderable (list_view);
}

/**
 * gtk_list_view_enable_model_drag_dest:
 * @list_view: a #GtkListView
 * @targets: (array length=n_targets): the table of targets that the drag will
 *           support
 * @n_targets: the number of items in @targets
 * @actions: the bitmask of possible actions for a drag to this
 *    widget
 *
 * Turns @list_view into a drop destination for automatic DND. Calling this
 * method sets #GtkListView:reorderable to %FALSE.
 *
 * Since: 2.8
 **/
void
gtk_list_view_enable_model_drag_dest (GtkListView          *list_view,
				      const GtkTargetEntry *targets,
				      gint                  n_targets,
				      GdkDragAction         actions)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  gtk_drag_dest_set (GTK_WIDGET (list_view), 0, targets, n_targets, actions);

  list_view->priv->dest_actions = actions;

  list_view->priv->dest_set = TRUE;

  unset_reorderable (list_view); 
}

/**
 * gtk_list_view_unset_model_drag_source:
 * @list_view: a #GtkListView
 *
 * Undoes the effect of gtk_list_view_enable_model_drag_source(). Calling this
 * method sets #GtkListView:reorderable to %FALSE.
 *
 * Since: 2.8
 **/
void
gtk_list_view_unset_model_drag_source (GtkListView *list_view)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  if (list_view->priv->source_set)
    {
      gtk_drag_source_unset (GTK_WIDGET (list_view));
      list_view->priv->source_set = FALSE;
    }

  unset_reorderable (list_view);
}

/**
 * gtk_list_view_unset_model_drag_dest:
 * @list_view: a #GtkListView
 *
 * Undoes the effect of gtk_list_view_enable_model_drag_dest(). Calling this
 * method sets #GtkListView:reorderable to %FALSE.
 *
 * Since: 2.8
 **/
void
gtk_list_view_unset_model_drag_dest (GtkListView *list_view)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  if (list_view->priv->dest_set)
    {
      gtk_drag_dest_unset (GTK_WIDGET (list_view));
      list_view->priv->dest_set = FALSE;
    }

  unset_reorderable (list_view);
}

/* These are useful to implement your own custom stuff. */
/**
 * gtk_list_view_set_drag_dest_item:
 * @list_view: a #GtkListView
 * @path: (allow-none): The path of the item to highlight, or %NULL.
 * @pos: Specifies where to drop, relative to the item
 *
 * Sets the item that is highlighted for feedback.
 *
 * Since: 2.8
 */
void
gtk_list_view_set_drag_dest_item (GtkListView              *list_view,
				  GtkTreePath              *path,
				  GtkListViewDropPosition   pos)
{
  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */

  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  if (list_view->priv->dest_item)
    {
      GtkTreePath *current_path;
      current_path = gtk_tree_row_reference_get_path (list_view->priv->dest_item);
      gtk_tree_row_reference_free (list_view->priv->dest_item);
      list_view->priv->dest_item = NULL;     

      gtk_list_view_queue_draw_path (list_view, current_path);
      gtk_tree_path_free (current_path);
    }
 
  /* special case a drop on an empty model */
  list_view->priv->empty_view_drop = FALSE;
  if (pos == GTK_LIST_VIEW_DROP_ABOVE && path
      && gtk_tree_path_get_depth (path) == 1
      && gtk_tree_path_get_indices (path)[0] == 0)
    {
      gint n_children;

      n_children = gtk_tree_model_iter_n_children (list_view->priv->model,
                                                   NULL);

      if (n_children == 0)
        list_view->priv->empty_view_drop = TRUE;
    }

  list_view->priv->dest_pos = pos;

  if (path)
    {
      list_view->priv->dest_item =
        gtk_tree_row_reference_new_proxy (G_OBJECT (list_view),
					  list_view->priv->model, path);
     
      gtk_list_view_queue_draw_path (list_view, path);
    }
}

/**
 * gtk_list_view_get_drag_dest_item:
 * @list_view: a #GtkListView
 * @path: (out) (allow-none): Return location for the path of
 *        the highlighted item, or %NULL.
 * @pos: (out) (allow-none): Return location for the drop position, or %NULL
 *
 * Gets information about the item that is highlighted for feedback.
 *
 * Since: 2.8
 **/
void
gtk_list_view_get_drag_dest_item (GtkListView              *list_view,
				  GtkTreePath             **path,
				  GtkListViewDropPosition  *pos)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  if (path)
    {
      if (list_view->priv->dest_item)
        *path = gtk_tree_row_reference_get_path (list_view->priv->dest_item);
      else
	*path = NULL;
    }

  if (pos)
    *pos = list_view->priv->dest_pos;
}

/**
 * gtk_list_view_get_dest_item_at_pos:
 * @list_view: a #GtkListView
 * @drag_x: the position to determine the destination item for
 * @drag_y: the position to determine the destination item for
 * @path: (out) (allow-none): Return location for the path of the item,
 *    or %NULL.
 * @pos: (out) (allow-none): Return location for the drop position, or %NULL
 *
 * Determines the destination item for a given position.
 *
 * Return value: whether there is an item at the given position.
 *
 * Since: 2.8
 **/
gboolean
gtk_list_view_get_dest_item_at_pos (GtkListView              *list_view,
				    gint                      drag_x,
				    gint                      drag_y,
				    GtkTreePath             **path,
				    GtkListViewDropPosition  *pos)
{
  GtkListViewItem *item;

  /* Note; this function is exported to allow a custom DND
   * implementation, so it can't touch TreeViewDragInfo
   */

  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), FALSE);
  g_return_val_if_fail (drag_x >= 0, FALSE);
  g_return_val_if_fail (drag_y >= 0, FALSE);
  g_return_val_if_fail (list_view->priv->bin_window != NULL, FALSE);


  if (path)
    *path = NULL;

  item = _gtk_list_view_get_item_at_coords (list_view,
					   drag_x + gtk_adjustment_get_value (list_view->priv->hadjustment),
					   drag_y + gtk_adjustment_get_value (list_view->priv->vadjustment),
					   FALSE, NULL);

  if (item == NULL)
    return FALSE;

  if (path)
    *path = gtk_tree_path_new_from_indices (item->index, -1);

  if (pos)
    {
      if (drag_x < item->cell_area.x + item->cell_area.width / 4)
	*pos = GTK_LIST_VIEW_DROP_LEFT;
      else if (drag_x > item->cell_area.x + item->cell_area.width * 3 / 4)
	*pos = GTK_LIST_VIEW_DROP_RIGHT;
      else if (drag_y < item->cell_area.y + item->cell_area.height / 4)
	*pos = GTK_LIST_VIEW_DROP_ABOVE;
      else if (drag_y > item->cell_area.y + item->cell_area.height * 3 / 4)
	*pos = GTK_LIST_VIEW_DROP_BELOW;
      else
	*pos = GTK_LIST_VIEW_DROP_INTO;
    }

  return TRUE;
}

/**
 * gtk_list_view_create_drag_list:
 * @list_view: a #GtkListView
 * @path: a #GtkTreePath in @list_view
 *
 * Creates a #cairo_surface_t representation of the item at @path. 
 * This image is used for a drag list.
 *
 * Return value: (transfer full): a newly-allocated surface of the drag list.
 *
 * Since: 2.8
 **/
cairo_surface_t *
gtk_list_view_create_drag_list (GtkListView *list_view,
				GtkTreePath *path)
{
  GtkWidget *widget;
  GtkStyleContext *context;
  cairo_t *cr;
  cairo_surface_t *surface;
  GList *l;
  gint index;

  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  widget = GTK_WIDGET (list_view);
  context = gtk_widget_get_style_context (widget);

  if (!gtk_widget_get_realized (widget))
    return NULL;

  index = gtk_tree_path_get_indices (path)[0];

  for (l = list_view->priv->items; l; l = l->next)
    {
      GtkListViewItem *item = l->data;
     
      if (index == item->index)
	{
	  GdkRectangle rect = {
	    item->cell_area.x - list_view->priv->item_padding,
	    item->cell_area.y - list_view->priv->item_padding,
	    item->cell_area.width  + list_view->priv->item_padding * 2,
	    item->cell_area.height + list_view->priv->item_padding * 2
	  };

	  surface = gdk_window_create_similar_surface (list_view->priv->bin_window,
                                                       CAIRO_CONTENT_COLOR,
                                                       rect.width + 2,
                                                       rect.height + 2);

	  cr = cairo_create (surface);
	  cairo_set_line_width (cr, 1.);

          gtk_render_background (context, cr, 0, 0,
                                 rect.width + 2, rect.height + 2);

          cairo_save (cr);

          cairo_rectangle (cr, 1, 1, rect.width, rect.height);
          cairo_clip (cr);

	  gtk_list_view_paint_item (list_view, cr, item,
				    list_view->priv->item_padding + 1,
				    list_view->priv->item_padding + 1, FALSE);

          cairo_restore (cr);

	  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0); /* black */
	  cairo_rectangle (cr, 0.5, 0.5, rect.width + 1, rect.height + 1);
	  cairo_stroke (cr);

	  cairo_destroy (cr);

	  return surface;
	}
    }
 
  return NULL;
}

/**
 * gtk_list_view_get_reorderable:
 * @list_view: a #GtkListView
 *
 * Retrieves whether the user can reorder the list via drag-and-drop.
 * See gtk_list_view_set_reorderable().
 *
 * Return value: %TRUE if the list can be reordered.
 *
 * Since: 2.8
 **/
gboolean
gtk_list_view_get_reorderable (GtkListView *list_view)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (list_view), FALSE);

  return list_view->priv->reorderable;
}

static const GtkTargetEntry item_targets[] = {
  { "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, 0 }
};


/**
 * gtk_list_view_set_reorderable:
 * @list_view: A #GtkListView.
 * @reorderable: %TRUE, if the list of items can be reordered.
 *
 * This function is a convenience function to allow you to reorder models that
 * support the #GtkTreeDragSourceIface and the #GtkTreeDragDestIface.  Both
 * #GtkTreeStore and #GtkListStore support these.  If @reorderable is %TRUE, then
 * the user can reorder the model by dragging and dropping rows.  The
 * developer can listen to these changes by connecting to the model's
 * row_inserted and row_deleted signals. The reordering is implemented by setting up
 * the list view as a drag source and destination. Therefore, drag and
 * drop can not be used in a reorderable view for any other purpose.
 *
 * This function does not give you any degree of control over the order -- any
 * reordering is allowed.  If more control is needed, you should probably
 * handle drag and drop manually.
 *
 * Since: 2.8
 **/
void
gtk_list_view_set_reorderable (GtkListView *list_view,
			       gboolean     reorderable)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (list_view));

  reorderable = reorderable != FALSE;

  if (list_view->priv->reorderable == reorderable)
    return;

  if (reorderable)
    {
      gtk_list_view_enable_model_drag_source (list_view,
					      GDK_BUTTON1_MASK,
					      item_targets,
					      G_N_ELEMENTS (item_targets),
					      GDK_ACTION_MOVE);
      gtk_list_view_enable_model_drag_dest (list_view,
					    item_targets,
					    G_N_ELEMENTS (item_targets),
					    GDK_ACTION_MOVE);
    }
  else
    {
      gtk_list_view_unset_model_drag_source (list_view);
      gtk_list_view_unset_model_drag_dest (list_view);
    }

  list_view->priv->reorderable = reorderable;

  g_object_notify (G_OBJECT (list_view), "reorderable");
}

static gboolean
gtk_list_view_buildable_custom_tag_start (GtkBuildable  *buildable,
                                          GtkBuilder    *builder,
                                          GObject       *child,
                                          const gchar   *tagname,
                                          GMarkupParser *parser,
                                          gpointer      *data)
{
  if (parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                tagname, parser, data))
    return TRUE;

  return _gtk_cell_layout_buildable_custom_tag_start (buildable, builder, child,
                                                      tagname, parser, data);
}

static void
gtk_list_view_buildable_custom_tag_end (GtkBuildable *buildable,
                                        GtkBuilder   *builder,
                                        GObject      *child,
                                        const gchar  *tagname,
                                        gpointer     *data)
{
  if (!_gtk_cell_layout_buildable_custom_tag_end (buildable, builder,
                                                  child, tagname, data))
    parent_buildable_iface->custom_tag_end (buildable, builder,
                                            child, tagname, data);
}
#endif
