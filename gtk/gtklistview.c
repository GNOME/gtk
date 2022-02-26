/*
 * Copyright © 2018 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtklistviewprivate.h"

#include "gtkbitset.h"
#include "gtkintl.h"
#include "gtklistbaseprivate.h"
#include "gtklistitemmanagerprivate.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkrbtreeprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkmultiselection.h"

/* Maximum number of list items created by the listview.
 * For debugging, you can set this to G_MAXUINT to ensure
 * there's always a list item for every row.
 */
#define GTK_LIST_VIEW_MAX_LIST_ITEMS 200

/* Extra items to keep above + below every tracker */
#define GTK_LIST_VIEW_EXTRA_ITEMS 2

/**
 * GtkListView:
 *
 * `GtkListView` presents a large dynamic list of items.
 *
 * `GtkListView` uses its factory to generate one row widget for each visible
 * item and shows them in a linear display, either vertically or horizontally.
 *
 * The [property@Gtk.ListView:show-separators] property offers a simple way to
 * display separators between the rows.
 *
 * `GtkListView` allows the user to select items according to the selection
 * characteristics of the model. For models that allow multiple selected items,
 * it is possible to turn on _rubberband selection_, using
 * [property@Gtk.ListView:enable-rubberband].
 *
 * If you need multiple columns with headers, see [class@Gtk.ColumnView].
 *
 * To learn more about the list widget framework, see the
 * [overview](section-list-widget.html).
 *
 * An example of using `GtkListView`:
 * ```c
 * static void
 * setup_listitem_cb (GtkListItemFactory *factory,
 *                    GtkListItem        *list_item)
 * {
 *   GtkWidget *image;
 *
 *   image = gtk_image_new ();
 *   gtk_image_set_icon_size (GTK_IMAGE (image), GTK_ICON_SIZE_LARGE);
 *   gtk_list_item_set_child (list_item, image);
 * }
 *
 * static void
 * bind_listitem_cb (GtkListItemFactory *factory,
 *                   GtkListItem        *list_item)
 * {
 *   GtkWidget *image;
 *   GAppInfo *app_info;
 *
 *   image = gtk_list_item_get_child (list_item);
 *   app_info = gtk_list_item_get_item (list_item);
 *   gtk_image_set_from_gicon (GTK_IMAGE (image), g_app_info_get_icon (app_info));
 * }
 *
 * static void
 * activate_cb (GtkListView  *list,
 *              guint         position,
 *              gpointer      unused)
 * {
 *   GAppInfo *app_info;
 *
 *   app_info = g_list_model_get_item (G_LIST_MODEL (gtk_list_view_get_model (list)), position);
 *   g_app_info_launch (app_info, NULL, NULL, NULL);
 *   g_object_unref (app_info);
 * }
 *
 * ...
 *
 *   model = create_application_list ();
 *
 *   factory = gtk_signal_list_item_factory_new ();
 *   g_signal_connect (factory, "setup", G_CALLBACK (setup_listitem_cb), NULL);
 *   g_signal_connect (factory, "bind", G_CALLBACK (bind_listitem_cb), NULL);
 *
 *   list = gtk_list_view_new (GTK_SELECTION_MODEL (gtk_single_selection_new (model)), factory);
 *
 *   g_signal_connect (list, "activate", G_CALLBACK (activate_cb), NULL);
 *
 *   gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);
 * ```
 *
 * # CSS nodes
 *
 * ```
 * listview[.separators][.rich-list][.navigation-sidebar][.data-table]
 * ├── row[.activatable]
 * │
 * ├── row[.activatable]
 * │
 * ┊
 * ╰── [rubberband]
 * ```
 *
 * `GtkListView` uses a single CSS node named `listview`. It may carry the
 * `.separators` style class, when [property@Gtk.ListView:show-separators]
 * property is set. Each child widget uses a single CSS node named `row`.
 * If the [property@Gtk.ListItem:activatable] property is set, the
 * corresponding row will have the `.activatable` style class. For
 * rubberband selection, a node with name `rubberband` is used.
 *
 * The main listview node may also carry style classes to select
 * the style of [list presentation](ListContainers.html#list-styles):
 * .rich-list, .navigation-sidebar or .data-table.
 *
 * # Accessibility
 *
 * `GtkListView` uses the %GTK_ACCESSIBLE_ROLE_LIST role, and the list
 * items use the %GTK_ACCESSIBLE_ROLE_LIST_ITEM role.
 */

typedef struct _ListRow ListRow;
typedef struct _ListRowAugment ListRowAugment;

struct _ListRow
{
  GtkListItemManagerItem parent;
  guint height; /* per row */
};

struct _ListRowAugment
{
  GtkListItemManagerItemAugment parent;
  guint height; /* total */
};

enum
{
  PROP_0,
  PROP_FACTORY,
  PROP_MODEL,
  PROP_SHOW_SEPARATORS,
  PROP_SINGLE_CLICK_ACTIVATE,
  PROP_ENABLE_RUBBERBAND,

  N_PROPS
};

enum {
  ACTIVATE,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkListView, gtk_list_view, GTK_TYPE_LIST_BASE)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void G_GNUC_UNUSED
dump (GtkListView *self)
{
  ListRow *row;
  guint n_widgets, n_list_rows;

  n_widgets = 0;
  n_list_rows = 0;
  //g_print ("ANCHOR: %u - %u\n", self->anchor_start, self->anchor_end);
  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget)
        n_widgets++;
      n_list_rows++;
      g_print ("  %4u%s (%upx)\n", row->parent.n_items, row->parent.widget ? " (widget)" : "", row->height);
    }

  g_print ("  => %u widgets in %u list rows\n", n_widgets, n_list_rows);
}

static void
list_row_augment (GtkRbTree *tree,
                  gpointer   node_augment,
                  gpointer   node,
                  gpointer   left,
                  gpointer   right)
{
  ListRow *row = node;
  ListRowAugment *aug = node_augment;

  gtk_list_item_manager_augment_node (tree, node_augment, node, left, right);

  aug->height = row->height * row->parent.n_items;

  if (left)
    {
      ListRowAugment *left_aug = gtk_rb_tree_get_augment (tree, left);

      aug->height += left_aug->height;
    }

  if (right)
    {
      ListRowAugment *right_aug = gtk_rb_tree_get_augment (tree, right);

      aug->height += right_aug->height;
    }
}

static ListRow *
gtk_list_view_get_row_at_y (GtkListView *self,
                            int          y,
                            int         *offset)
{
  ListRow *row, *tmp;

  row = gtk_list_item_manager_get_root (self->item_manager);

  while (row)
    {
      tmp = gtk_rb_tree_node_get_left (row);
      if (tmp)
        {
          ListRowAugment *aug = gtk_list_item_manager_get_item_augment (self->item_manager, tmp);
          if (y < aug->height)
            {
              row = tmp;
              continue;
            }
          y -= aug->height;
        }

      if (y < row->height * row->parent.n_items)
        break;
      y -= row->height * row->parent.n_items;

      row = gtk_rb_tree_node_get_right (row);
    }

  if (offset)
    *offset = row ? y : 0;

  return row;
}

static int
list_row_get_y (GtkListView *self,
                ListRow     *row)
{
  ListRow *parent, *left;
  int y;

  left = gtk_rb_tree_node_get_left (row);
  if (left)
    {
      ListRowAugment *aug = gtk_list_item_manager_get_item_augment (self->item_manager, left);
      y = aug->height;
    }
  else
    y = 0; 

  for (parent = gtk_rb_tree_node_get_parent (row);
       parent != NULL;
       parent = gtk_rb_tree_node_get_parent (row))
    {
      left = gtk_rb_tree_node_get_left (parent);

      if (left != row)
        {
          if (left)
            {
              ListRowAugment *aug = gtk_list_item_manager_get_item_augment (self->item_manager, left);
              y += aug->height;
            }
          y += parent->height * parent->parent.n_items;
        }

      row = parent;
    }

  return y ;
}

static int
gtk_list_view_get_list_height (GtkListView *self)
{
  ListRow *row;
  ListRowAugment *aug;

  row = gtk_list_item_manager_get_root (self->item_manager);
  if (row == NULL)
    return 0;

  aug = gtk_list_item_manager_get_item_augment (self->item_manager, row);
  return aug->height;
}

static gboolean
gtk_list_view_get_allocation_along (GtkListBase *base,
                                    guint        pos,
                                    int         *offset,
                                    int         *size)
{
  GtkListView *self = GTK_LIST_VIEW (base);
  ListRow *row;
  guint skip;
  int y;

  row = gtk_list_item_manager_get_nth (self->item_manager, pos, &skip);
  if (row == NULL)
    {
      if (offset)
        *offset = 0;
      if (size)
        *size = 0;
      return FALSE;
    }

  y = list_row_get_y (self, row);
  y += skip * row->height;

  if (offset)
    *offset = y;
  if (size)
    *size = row->height;

  return TRUE;
}

static gboolean
gtk_list_view_get_allocation_across (GtkListBase *base,
                                     guint        pos,
                                     int         *offset,
                                     int         *size)
{
  GtkListView *self = GTK_LIST_VIEW (base);

  if (offset)
    *offset = 0;
  if (size)
    *size = self->list_width;

  return TRUE;
}

static GtkBitset *
gtk_list_view_get_items_in_rect (GtkListBase                 *base,
                                 const cairo_rectangle_int_t *rect)
{
  GtkListView *self = GTK_LIST_VIEW (base);
  guint first, last, n_items;
  GtkBitset *result;
  ListRow *row;

  result = gtk_bitset_new_empty ();

  n_items = gtk_list_base_get_n_items (base);
  if (n_items == 0)
    return result;

  row = gtk_list_view_get_row_at_y (self, rect->y, NULL);
  if (row)
    first = gtk_list_item_manager_get_item_position (self->item_manager, row);
  else
    first = rect->y < 0 ? 0 : n_items - 1;
  row = gtk_list_view_get_row_at_y (self, rect->y + rect->height, NULL);
  if (row)
    last = gtk_list_item_manager_get_item_position (self->item_manager, row);
  else
    last = rect->y < 0 ? 0 : n_items - 1;

  gtk_bitset_add_range_closed (result, first, last);
  return result;
}

static guint
gtk_list_view_move_focus_along (GtkListBase *base,
                                guint        pos,
                                int          steps)
{
  if (steps < 0)
    return pos - MIN (pos, -steps);
  else
    {
      pos += MIN (gtk_list_base_get_n_items (base) - pos - 1, steps);
    }

  return pos;
}

static gboolean
gtk_list_view_get_position_from_allocation (GtkListBase           *base,
                                            int                    across,
                                            int                    along,
                                            guint                 *pos,
                                            cairo_rectangle_int_t *area)
{
  GtkListView *self = GTK_LIST_VIEW (base);
  ListRow *row;
  int remaining;

  if (across >= self->list_width)
    return FALSE;

  row = gtk_list_view_get_row_at_y (self, along, &remaining);
  if (row == NULL)
    return FALSE;

  *pos = gtk_list_item_manager_get_item_position (self->item_manager, row);
  g_assert (remaining < row->height * row->parent.n_items);
  *pos += remaining / row->height;

  if (area)
    {
      area->x = 0;
      area->width = self->list_width;
      area->y = along - remaining % row->height;
      area->height = row->height;
    }

  return TRUE;
}

static guint
gtk_list_view_move_focus_across (GtkListBase *base,
                                 guint        pos,
                                 int          steps)
{
  return pos;
}

static int
compare_ints (gconstpointer first,
               gconstpointer second)
{
  return *(int *) first - *(int *) second;
}

static guint
gtk_list_view_get_unknown_row_height (GtkListView *self,
                                      GArray      *heights)
{
  g_return_val_if_fail (heights->len > 0, 0);

  /* return the median and hope rows are generally uniform with few outliers */
  g_array_sort (heights, compare_ints);

  return g_array_index (heights, int, heights->len / 2);
}

static void
gtk_list_view_measure_across (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  ListRow *row;
  int min, nat, child_min, child_nat;
  /* XXX: Figure out how to split a given height into per-row heights.
   * Good luck! */
  for_size = -1;

  min = 0;
  nat = 0;

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      /* ignore unavailable rows */
      if (row->parent.widget == NULL)
        continue;

      gtk_widget_measure (row->parent.widget,
                          orientation, for_size,
                          &child_min, &child_nat, NULL, NULL);
      min = MAX (min, child_min);
      nat = MAX (nat, child_nat);
    }

  *minimum = min;
  *natural = nat;
}

static void
gtk_list_view_measure_list (GtkWidget      *widget,
                            GtkOrientation  orientation,
                            int             for_size,
                            int            *minimum,
                            int            *natural)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  ListRow *row;
  int min, nat, child_min, child_nat;
  GArray *min_heights, *nat_heights;
  guint n_unknown;

  min_heights = g_array_new (FALSE, FALSE, sizeof (int));
  nat_heights = g_array_new (FALSE, FALSE, sizeof (int));
  n_unknown = 0;
  min = 0;
  nat = 0;

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget)
        {
          gtk_widget_measure (row->parent.widget,
                              orientation, for_size,
                              &child_min, &child_nat, NULL, NULL);
          g_array_append_val (min_heights, child_min);
          g_array_append_val (nat_heights, child_nat);
          min += child_min;
          nat += child_nat;
        }
      else
        {
          n_unknown += row->parent.n_items;
        }
    }

  if (n_unknown)
    {
      min += n_unknown * gtk_list_view_get_unknown_row_height (self, min_heights);
      nat += n_unknown * gtk_list_view_get_unknown_row_height (self, nat_heights);
    }
  g_array_free (min_heights, TRUE);
  g_array_free (nat_heights, TRUE);

  *minimum = min;
  *natural = nat;
}

static void
gtk_list_view_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkListView *self = GTK_LIST_VIEW (widget);

  if (orientation == gtk_list_base_get_orientation (GTK_LIST_BASE (self)))
    gtk_list_view_measure_list (widget, orientation, for_size, minimum, natural);
  else
    gtk_list_view_measure_across (widget, orientation, for_size, minimum, natural);
}

static void
gtk_list_view_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  ListRow *row;
  GArray *heights;
  int min, nat, row_height;
  int x, y;
  GtkOrientation orientation, opposite_orientation;
  GtkScrollablePolicy scroll_policy, opposite_scroll_policy;

  orientation = gtk_list_base_get_orientation (GTK_LIST_BASE (self));
  opposite_orientation = OPPOSITE_ORIENTATION (orientation);
  scroll_policy = gtk_list_base_get_scroll_policy (GTK_LIST_BASE (self), orientation);
  opposite_scroll_policy = gtk_list_base_get_scroll_policy (GTK_LIST_BASE (self), opposite_orientation);

  /* step 0: exit early if list is empty */
  if (gtk_list_item_manager_get_root (self->item_manager) == NULL)
    return;

  /* step 1: determine width of the list */
  gtk_widget_measure (widget, opposite_orientation,
                      -1,
                      &min, &nat, NULL, NULL);
  self->list_width = orientation == GTK_ORIENTATION_VERTICAL ? width : height;
  if (opposite_scroll_policy == GTK_SCROLL_MINIMUM)
    self->list_width = MAX (min, self->list_width);
  else
    self->list_width = MAX (nat, self->list_width);

  /* step 2: determine height of known list items */
  heights = g_array_new (FALSE, FALSE, sizeof (int));

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget == NULL)
        continue;

      gtk_widget_measure (row->parent.widget, orientation,
                          self->list_width,
                          &min, &nat, NULL, NULL);
      if (scroll_policy == GTK_SCROLL_MINIMUM)
        row_height = min;
      else
        row_height = nat;
      if (row->height != row_height)
        {
          row->height = row_height;
          gtk_rb_tree_node_mark_dirty (row);
        }
      g_array_append_val (heights, row_height);
    }

  /* step 3: determine height of unknown items */
  row_height = gtk_list_view_get_unknown_row_height (self, heights);
  g_array_free (heights, TRUE);

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget)
        continue;

      if (row->height != row_height)
        {
          row->height = row_height;
          gtk_rb_tree_node_mark_dirty (row);
        }
    }

  /* step 3: update the adjustments */
  gtk_list_base_update_adjustments (GTK_LIST_BASE (self),
                                    self->list_width,
                                    gtk_list_view_get_list_height (self),
                                    gtk_widget_get_size (widget, opposite_orientation),
                                    gtk_widget_get_size (widget, orientation),
                                    &x, &y);
  x = -x;
  y = -y;

  /* step 4: actually allocate the widgets */

  for (row = gtk_list_item_manager_get_first (self->item_manager);
       row != NULL;
       row = gtk_rb_tree_node_get_next (row))
    {
      if (row->parent.widget)
        {
          gtk_list_base_size_allocate_child (GTK_LIST_BASE (self),
                                             row->parent.widget,
                                             x,
                                             y,
                                             self->list_width,
                                             row->height);
        }

      y += row->height * row->parent.n_items;
    }

  gtk_list_base_allocate_rubberband (GTK_LIST_BASE (self));
}

static void
gtk_list_view_dispose (GObject *object)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  self->item_manager = NULL;

  G_OBJECT_CLASS (gtk_list_view_parent_class)->dispose (object);
}

static void
gtk_list_view_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      g_value_set_object (value, gtk_list_item_manager_get_factory (self->item_manager));
      break;

    case PROP_MODEL:
      g_value_set_object (value, gtk_list_base_get_model (GTK_LIST_BASE (self)));
      break;

    case PROP_SHOW_SEPARATORS:
      g_value_set_boolean (value, self->show_separators);
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      g_value_set_boolean (value, gtk_list_item_manager_get_single_click_activate (self->item_manager));
      break;

    case PROP_ENABLE_RUBBERBAND:
      g_value_set_boolean (value, gtk_list_base_get_enable_rubberband (GTK_LIST_BASE (self)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_view_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkListView *self = GTK_LIST_VIEW (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      gtk_list_view_set_factory (self, g_value_get_object (value));
      break;

    case PROP_MODEL:
      gtk_list_view_set_model (self, g_value_get_object (value));
      break;

    case PROP_SHOW_SEPARATORS:
      gtk_list_view_set_show_separators (self, g_value_get_boolean (value));
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      gtk_list_view_set_single_click_activate (self, g_value_get_boolean (value));
      break;

    case PROP_ENABLE_RUBBERBAND:
      gtk_list_view_set_enable_rubberband (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_view_activate_item (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameter)
{
  GtkListView *self = GTK_LIST_VIEW (widget);
  guint pos;

  if (!g_variant_check_format_string (parameter, "u", FALSE))
    return;

  g_variant_get (parameter, "u", &pos);
  if (pos >= gtk_list_base_get_n_items (GTK_LIST_BASE (self)))
    return;

  g_signal_emit (widget, signals[ACTIVATE], 0, pos);
}

static void
gtk_list_view_class_init (GtkListViewClass *klass)
{
  GtkListBaseClass *list_base_class = GTK_LIST_BASE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  list_base_class->list_item_name = "row";
  list_base_class->list_item_role = GTK_ACCESSIBLE_ROLE_LIST_ITEM;
  list_base_class->list_item_size = sizeof (ListRow);
  list_base_class->list_item_augment_size = sizeof (ListRowAugment);
  list_base_class->list_item_augment_func = list_row_augment;
  list_base_class->get_allocation_along = gtk_list_view_get_allocation_along;
  list_base_class->get_allocation_across = gtk_list_view_get_allocation_across;
  list_base_class->get_items_in_rect = gtk_list_view_get_items_in_rect;
  list_base_class->get_position_from_allocation = gtk_list_view_get_position_from_allocation;
  list_base_class->move_focus_along = gtk_list_view_move_focus_along;
  list_base_class->move_focus_across = gtk_list_view_move_focus_across;

  widget_class->measure = gtk_list_view_measure;
  widget_class->size_allocate = gtk_list_view_size_allocate;

  gobject_class->dispose = gtk_list_view_dispose;
  gobject_class->get_property = gtk_list_view_get_property;
  gobject_class->set_property = gtk_list_view_set_property;

  /**
   * GtkListView:factory: (attributes org.gtk.Property.get=gtk_list_view_get_factory org.gtk.Property.set=gtk_list_view_set_factory)
   *
   * Factory for populating list items.
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory",
                         P_("Factory"),
                         P_("Factory for populating list items"),
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListView:model: (attributes org.gtk.Property.get=gtk_list_view_get_model org.gtk.Property.set=gtk_list_view_set_model)
   *
   * Model for the items displayed.
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         P_("Model"),
                         P_("Model for the items displayed"),
                         GTK_TYPE_SELECTION_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkListView:show-separators: (attributes org.gtk.Property.get=gtk_list_view_get_show_separators org.gtk.Property.set=gtk_list_view_set_show_separators)
   *
   * Show separators between rows.
   */
  properties[PROP_SHOW_SEPARATORS] =
    g_param_spec_boolean ("show-separators",
                          P_("Show separators"),
                          P_("Show separators between rows"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkListView:single-click-activate: (attributes org.gtk.Property.get=gtk_list_view_get_single_click_activate org.gtk.Property.set=gtk_list_view_set_single_click_activate)
   *
   * Activate rows on single click and select them on hover.
   */
  properties[PROP_SINGLE_CLICK_ACTIVATE] =
    g_param_spec_boolean ("single-click-activate",
                          P_("Single click activate"),
                          P_("Activate rows on single click"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkListView:enable-rubberband: (attributes org.gtk.Property.get=gtk_list_view_get_enable_rubberband org.gtk.Property.set=gtk_list_view_set_enable_rubberband)
   *
   * Allow rubberband selection.
   */
  properties[PROP_ENABLE_RUBBERBAND] =
    g_param_spec_boolean ("enable-rubberband",
                          P_("Enable rubberband selection"),
                          P_("Allow selecting items by dragging with the mouse"),
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /**
   * GtkListView::activate:
   * @self: The `GtkListView`
   * @position: position of item to activate
   *
   * Emitted when a row has been activated by the user,
   * usually via activating the GtkListView|list.activate-item action.
   *
   * This allows for a convenient way to handle activation in a listview.
   * See [method@Gtk.ListItem.set_activatable] for details on how to use
   * this signal.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 1,
                  G_TYPE_UINT);
  g_signal_set_va_marshaller (signals[ACTIVATE],
                              G_TYPE_FROM_CLASS (gobject_class),
                              g_cclosure_marshal_VOID__UINTv);

  /**
   * GtkListView|list.activate-item:
   * @position: position of item to activate
   *
   * Activates the item given in @position by emitting the
   * [signal@Gtk.ListView::activate] signal.
   */
  gtk_widget_class_install_action (widget_class,
                                   "list.activate-item",
                                   "u",
                                   gtk_list_view_activate_item);

  gtk_widget_class_set_css_name (widget_class, I_("listview"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_LIST);
}

static void
gtk_list_view_init (GtkListView *self)
{
  self->item_manager = gtk_list_base_get_manager (GTK_LIST_BASE (self));

  gtk_list_base_set_anchor_max_widgets (GTK_LIST_BASE (self),
                                        GTK_LIST_VIEW_MAX_LIST_ITEMS,
                                        GTK_LIST_VIEW_EXTRA_ITEMS);

  gtk_widget_add_css_class (GTK_WIDGET (self), "view");
}

/**
 * gtk_list_view_new:
 * @model: (nullable) (transfer full): the model to use
 * @factory: (nullable) (transfer full): The factory to populate items with
 *
 * Creates a new `GtkListView` that uses the given @factory for
 * mapping items to widgets.
 *
 * The function takes ownership of the
 * arguments, so you can write code like
 * ```c
 * list_view = gtk_list_view_new (create_model (),
 *   gtk_builder_list_item_factory_new_from_resource ("/resource.ui"));
 * ```
 *
 * Returns: a new `GtkListView` using the given @model and @factory
 */
GtkWidget *
gtk_list_view_new (GtkSelectionModel  *model,
                   GtkListItemFactory *factory)
{
  GtkWidget *result;

  g_return_val_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model), NULL);
  g_return_val_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory), NULL);

  result = g_object_new (GTK_TYPE_LIST_VIEW,
                         "model", model,
                         "factory", factory,
                         NULL);

  /* consume the references */
  g_clear_object (&model);
  g_clear_object (&factory);

  return result;
}

/**
 * gtk_list_view_get_model: (attributes org.gtk.Method.get_property=model)
 * @self: a `GtkListView`
 *
 * Gets the model that's currently used to read the items displayed.
 *
 * Returns: (nullable) (transfer none): The model in use
 */
GtkSelectionModel *
gtk_list_view_get_model (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), NULL);

  return gtk_list_base_get_model (GTK_LIST_BASE (self));
}

/**
 * gtk_list_view_set_model: (attributes org.gtk.Method.set_property=model)
 * @self: a `GtkListView`
 * @model: (nullable) (transfer none): the model to use
 *
 * Sets the model to use.
 *
 * This must be a [iface@Gtk.SelectionModel] to use.
 */
void
gtk_list_view_set_model (GtkListView       *self,
                         GtkSelectionModel *model)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (model == NULL || GTK_IS_SELECTION_MODEL (model));

  if (!gtk_list_base_set_model (GTK_LIST_BASE (self), model))
    return;

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE, GTK_IS_MULTI_SELECTION (model),
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_list_view_get_factory: (attributes org.gtk.Method.get_property=factory)
 * @self: a `GtkListView`
 *
 * Gets the factory that's currently used to populate list items.
 *
 * Returns: (nullable) (transfer none): The factory in use
 */
GtkListItemFactory *
gtk_list_view_get_factory (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), NULL);

  return gtk_list_item_manager_get_factory (self->item_manager);
}

/**
 * gtk_list_view_set_factory: (attributes org.gtk.Method.set_property=factory)
 * @self: a `GtkListView`
 * @factory: (nullable) (transfer none): the factory to use
 *
 * Sets the `GtkListItemFactory` to use for populating list items.
 */
void
gtk_list_view_set_factory (GtkListView        *self,
                           GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));
  g_return_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory));

  if (factory == gtk_list_item_manager_get_factory (self->item_manager))
    return;

  gtk_list_item_manager_set_factory (self->item_manager, factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

/**
 * gtk_list_view_set_show_separators: (attributes org.gtk.Method.set_property=show-separators)
 * @self: a `GtkListView`
 * @show_separators: %TRUE to show separators
 *
 * Sets whether the list box should show separators
 * between rows.
 */
void
gtk_list_view_set_show_separators (GtkListView *self,
                                   gboolean     show_separators)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));

  if (self->show_separators == show_separators)
    return;

  self->show_separators = show_separators;

  if (show_separators)
    gtk_widget_add_css_class (GTK_WIDGET (self), "separators");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "separators");

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SEPARATORS]);
}

/**
 * gtk_list_view_get_show_separators: (attributes org.gtk.Method.get_property=show-separators)
 * @self: a `GtkListView`
 *
 * Returns whether the list box should show separators
 * between rows.
 *
 * Returns: %TRUE if the list box shows separators
 */
gboolean
gtk_list_view_get_show_separators (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), FALSE);

  return self->show_separators;
}

/**
 * gtk_list_view_set_single_click_activate: (attributes org.gtk.Method.set_property=single-click-activate)
 * @self: a `GtkListView`
 * @single_click_activate: %TRUE to activate items on single click
 *
 * Sets whether rows should be activated on single click and
 * selected on hover.
 */
void
gtk_list_view_set_single_click_activate (GtkListView *self,
                                         gboolean     single_click_activate)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));

  if (single_click_activate == gtk_list_item_manager_get_single_click_activate (self->item_manager))
    return;

  gtk_list_item_manager_set_single_click_activate (self->item_manager, single_click_activate);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SINGLE_CLICK_ACTIVATE]);
}

/**
 * gtk_list_view_get_single_click_activate: (attributes org.gtk.Method.set_property=single-click-activate)
 * @self: a `GtkListView`
 *
 * Returns whether rows will be activated on single click and
 * selected on hover.
 *
 * Returns: %TRUE if rows are activated on single click
 */
gboolean
gtk_list_view_get_single_click_activate (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), FALSE);

  return gtk_list_item_manager_get_single_click_activate (self->item_manager);
}

/**
 * gtk_list_view_set_enable_rubberband: (attributes org.gtk.Method.set_property=enable-rubberband)
 * @self: a `GtkListView`
 * @enable_rubberband: %TRUE to enable rubberband selection
 *
 * Sets whether selections can be changed by dragging with the mouse.
 */
void
gtk_list_view_set_enable_rubberband (GtkListView *self,
                                     gboolean     enable_rubberband)
{
  g_return_if_fail (GTK_IS_LIST_VIEW (self));

  if (enable_rubberband == gtk_list_base_get_enable_rubberband (GTK_LIST_BASE (self)))
    return;

  gtk_list_base_set_enable_rubberband (GTK_LIST_BASE (self), enable_rubberband);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLE_RUBBERBAND]);
}

/**
 * gtk_list_view_get_enable_rubberband: (attributes org.gtk.Method.get_property=enable-rubberband)
 * @self: a `GtkListView`
 *
 * Returns whether rows can be selected by dragging with the mouse.
 *
 * Returns: %TRUE if rubberband selection is enabled
 */
gboolean
gtk_list_view_get_enable_rubberband (GtkListView *self)
{
  g_return_val_if_fail (GTK_IS_LIST_VIEW (self), FALSE);

  return gtk_list_base_get_enable_rubberband (GTK_LIST_BASE (self));
}
