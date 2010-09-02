/* gtkwrapbox.c
 * Copyright (C) 2007-2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


/**
 * SECTION:gtkwrapbox
 * @Short_Description: A container that wraps its children;
 * @Title: GtkWrapBox
 *
 * #GtkWrapBox allocates space for an ordered list of children
 * by wrapping them over in the box's orentation.
 *
 */

#include "config.h"
#include "gtksizerequest.h"
#include "gtkorientable.h"
#include "gtkwrapbox.h"
#include "gtkprivate.h"
#include "gtkintl.h"


typedef struct _GtkWrapBoxChild  GtkWrapBoxChild;

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_ALLOCATION_MODE,
  PROP_SPREADING,
  PROP_HORIZONTAL_SPACING,
  PROP_VERTICAL_SPACING,
  PROP_MINIMUM_LINE_CHILDREN,
  PROP_NATURAL_LINE_CHILDREN
};

enum
{
  CHILD_PROP_0,
  CHILD_PROP_X_EXPAND,
  CHILD_PROP_X_FILL,
  CHILD_PROP_Y_EXPAND,
  CHILD_PROP_Y_FILL,
  CHILD_PROP_X_PADDING,
  CHILD_PROP_Y_PADDING
};

struct _GtkWrapBoxPrivate
{
  GtkOrientation        orientation;
  GtkWrapAllocationMode mode;
  GtkWrapBoxSpreading   spreading;

  guint16               vertical_spacing;
  guint16               horizontal_spacing;

  guint16               minimum_line_children;
  guint16               natural_line_children;

  GList                *children;
};

struct _GtkWrapBoxChild
{
  GtkWidget *widget;

  guint16    xpadding;
  guint16    ypadding;

  guint16    xexpand : 1;
  guint16    xfill   : 1;
  guint16    yexpand : 1;
  guint16    yfill   : 1;
};

/* GObjectClass */
static void gtk_wrap_box_get_property         (GObject             *object,
                                               guint                prop_id,
                                               GValue              *value,
                                               GParamSpec          *pspec);
static void gtk_wrap_box_set_property         (GObject             *object,
                                               guint                prop_id,
                                               const GValue        *value,
                                               GParamSpec          *pspec);

/* GtkWidgetClass */
static void gtk_wrap_box_size_allocate        (GtkWidget           *widget,
                                               GtkAllocation       *allocation);

/* GtkContainerClass */
static void gtk_wrap_box_add                  (GtkContainer        *container,
                                               GtkWidget           *widget);
static void gtk_wrap_box_remove               (GtkContainer        *container,
                                               GtkWidget           *widget);
static void gtk_wrap_box_forall               (GtkContainer        *container,
                                               gboolean             include_internals,
                                               GtkCallback          callback,
                                               gpointer             callback_data);
static void gtk_wrap_box_set_child_property   (GtkContainer        *container,
                                               GtkWidget           *child,
                                               guint                property_id,
                                               const GValue        *value,
                                               GParamSpec          *pspec);
static void gtk_wrap_box_get_child_property   (GtkContainer        *container,
                                               GtkWidget           *child,
                                               guint                property_id,
                                               GValue              *value,
                                               GParamSpec          *pspec);
static GType gtk_wrap_box_child_type          (GtkContainer        *container);


/* GtkSizeRequest */
static void gtk_wrap_box_size_request_init    (GtkSizeRequestIface *iface);
static GtkSizeRequestMode gtk_wrap_box_get_request_mode (GtkSizeRequest *widget);
static void gtk_wrap_box_get_width            (GtkSizeRequest      *widget,
                                               gint                *minimum_size,
                                               gint                *natural_size);
static void gtk_wrap_box_get_height           (GtkSizeRequest      *widget,
                                               gint                *minimum_size,
                                               gint                *natural_size);
static void gtk_wrap_box_get_height_for_width (GtkSizeRequest      *box,
                                               gint                 width,
                                               gint                *minimum_height,
                                               gint                *natural_height);
static void gtk_wrap_box_get_width_for_height (GtkSizeRequest      *box,
                                               gint                 width,
                                               gint                *minimum_height,
                                               gint                *natural_height);


G_DEFINE_TYPE_WITH_CODE (GtkWrapBox, gtk_wrap_box, GTK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SIZE_REQUEST,
                                                gtk_wrap_box_size_request_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))


static void
gtk_wrap_box_class_init (GtkWrapBoxClass *class)
{
  GObjectClass      *gobject_class    = G_OBJECT_CLASS (class);
  GtkWidgetClass    *widget_class     = GTK_WIDGET_CLASS (class);
  GtkContainerClass *container_class  = GTK_CONTAINER_CLASS (class);

  gobject_class->get_property         = gtk_wrap_box_get_property;
  gobject_class->set_property         = gtk_wrap_box_set_property;

  widget_class->size_allocate         = gtk_wrap_box_size_allocate;

  container_class->add                = gtk_wrap_box_add;
  container_class->remove             = gtk_wrap_box_remove;
  container_class->forall             = gtk_wrap_box_forall;
  container_class->child_type         = gtk_wrap_box_child_type;
  container_class->set_child_property = gtk_wrap_box_set_child_property;
  container_class->get_child_property = gtk_wrap_box_get_child_property;

  /* GObjectClass properties */
  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");

  /**
   * GtkWrapBox:allocation-mode:
   *
   * The #GtkWrapAllocationMode to use.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ALLOCATION_MODE,
                                   g_param_spec_enum ("allocation-mode",
                                                      P_("Allocation Mode"),
                                                      P_("The allocation mode to use"),
                                                      GTK_TYPE_WRAP_ALLOCATION_MODE,
                                                      GTK_WRAP_ALLOCATE_FREE,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkWrapBox:spreading:
   *
   * The #GtkWrapBoxSpreading to used to define what is done with extra
   * space.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SPREADING,
                                   g_param_spec_enum ("spreading",
                                                      P_("Spreading"),
                                                      P_("The spreading mode to use"),
                                                      GTK_TYPE_WRAP_BOX_SPREADING,
                                                      GTK_WRAP_BOX_SPREAD_BEGIN,
                                                      GTK_PARAM_READWRITE));


  /**
   * GtkWrapBox:minimum-line-children:
   *
   * The minimum number of children to allocate consecutively in the given orientation.
   *
   * <note><para>Setting the minimum children per line ensures
   * that a reasonably small height will be requested
   * for the overall minimum width of the box.</para></note>
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MINIMUM_LINE_CHILDREN,
                                   g_param_spec_uint ("minimum-line-children",
                                                      P_("Minimum Line Children"),
                                                      P_("The minimum number of children to allocate "
                                                        "consecutively in the given orientation."),
                                                      0,
                                                      65535,
                                                      0,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkWrapBox:natural-line-children:
   *
   * The maximum amount of children to request space for consecutively in the given orientation.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_NATURAL_LINE_CHILDREN,
                                   g_param_spec_uint ("natural-line-children",
                                                      P_("Natural Line Children"),
                                                      P_("The maximum amount of children to request space for "
                                                        "consecutively in the given orientation."),
                                                      0,
                                                      65535,
                                                      0,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkWrapBox:vertical-spacing:
   *
   * The amount of vertical space between two children.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VERTICAL_SPACING,
                                   g_param_spec_uint ("vertical-spacing",
                                                     P_("Vertical spacing"),
                                                     P_("The amount of vertical space between two children"),
                                                     0,
                                                     65535,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkWrapBox:horizontal-spacing:
   *
   * The amount of horizontal space between two children.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HORIZONTAL_SPACING,
                                   g_param_spec_uint ("horizontal-spacing",
                                                     P_("Horizontal spacing"),
                                                     P_("The amount of horizontal space between two children"),
                                                     0,
                                                     65535,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /* GtkContainerClass child properties */

  /**
   * GtkWrapBox:x-expand:
   *
   * Whether the child expands horizontally.
   *
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_X_EXPAND,
                                              g_param_spec_boolean
                                              ("x-expand",
                                               P_("Horizontally expand"),
                                               P_("Whether the child expands horizontally."),
                                               FALSE,
                                               GTK_PARAM_READWRITE));

  /**
   * GtkWrapBox:x-fill:
   *
   * Whether the child fills its allocated horizontal space.
   *
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_X_FILL,
                                              g_param_spec_boolean
                                              ("x-fill",
                                               P_("Horizontally fills"),
                                               P_("Whether the child fills its allocated horizontal space."),
                                               FALSE,
                                               GTK_PARAM_READWRITE));

  /**
   * GtkWrapBox:y-expand:
   *
   * Whether the child expands vertically.
   *
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_Y_EXPAND,
                                              g_param_spec_boolean
                                              ("y-expand",
                                               P_("Vertically expand"),
                                               P_("Whether the child expands vertically."),
                                               FALSE,
                                               GTK_PARAM_READWRITE));

  /**
   * GtkWrapBox:y-fill:
   *
   * Whether the child fills its allocated vertical space.
   *
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_Y_FILL,
                                              g_param_spec_boolean
                                              ("y-fill",
                                               P_("Vertically fills"),
                                               P_("Whether the child fills its allocated vertical space."),
                                               FALSE,
                                               GTK_PARAM_READWRITE));

  /**
   * GtkWrapBox:x-padding:
   *
   * Extra space to put between the child and its left and right neighbors, in pixels.
   *
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_X_PADDING,
                                              g_param_spec_uint
                                              ("x-padding",
                                               P_("Horizontal padding"),
                                               P_("Extra space to put between the child and "
                                                  "its left and right neighbors, in pixels"),
                                               0, 65535, 0,
                                               GTK_PARAM_READWRITE));


  /**
   * GtkWrapBox:y-padding:
   *
   * Extra space to put between the child and its upper and lower neighbors, in pixels.
   *
   */
  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_Y_PADDING,
                                              g_param_spec_uint
                                              ("y-padding",
                                               P_("Vertical padding"),
                                               P_("Extra space to put between the child and "
                                                  "its upper and lower neighbors, in pixels"),
                                               0, 65535, 0,
                                               GTK_PARAM_READWRITE));

  g_type_class_add_private (class, sizeof (GtkWrapBoxPrivate));
}

static void
gtk_wrap_box_init (GtkWrapBox *box)
{
  GtkWrapBoxPrivate *priv;

  box->priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (box, GTK_TYPE_WRAP_BOX, GtkWrapBoxPrivate);

  priv->orientation        = GTK_ORIENTATION_HORIZONTAL;
  priv->mode               = GTK_WRAP_ALLOCATE_FREE;
  priv->spreading          = GTK_WRAP_BOX_SPREAD_BEGIN;
  priv->vertical_spacing   = 0;
  priv->horizontal_spacing = 0;
  priv->children           = NULL;

  gtk_widget_set_has_window (GTK_WIDGET (box), FALSE);
}

/*****************************************************
 *                  GObectClass                      *
 *****************************************************/
static void
gtk_wrap_box_get_property (GObject      *object,
                           guint         prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  GtkWrapBox        *box  = GTK_WRAP_BOX (object);
  GtkWrapBoxPrivate *priv = box->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_boolean (value, priv->orientation);
      break;
    case PROP_ALLOCATION_MODE:
      g_value_set_enum (value, priv->mode);
      break;
    case PROP_SPREADING:
      g_value_set_enum (value, priv->spreading);
      break;
    case PROP_VERTICAL_SPACING:
      g_value_set_uint (value, priv->vertical_spacing);
      break;
    case PROP_HORIZONTAL_SPACING:
      g_value_set_uint (value, priv->horizontal_spacing);
      break;
    case PROP_MINIMUM_LINE_CHILDREN:
      g_value_set_uint (value, priv->minimum_line_children);
      break;
    case PROP_NATURAL_LINE_CHILDREN:
      g_value_set_uint (value, priv->natural_line_children);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_wrap_box_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkWrapBox        *box = GTK_WRAP_BOX (object);
  GtkWrapBoxPrivate *priv   = box->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      priv->orientation = g_value_get_enum (value);

      /* Re-box the children in the new orientation */
      gtk_widget_queue_resize (GTK_WIDGET (box));
      break;
    case PROP_ALLOCATION_MODE:
      gtk_wrap_box_set_allocation_mode (box, g_value_get_enum (value));
      break;
    case PROP_SPREADING:
      gtk_wrap_box_set_spreading (box, g_value_get_enum (value));
      break;
    case PROP_VERTICAL_SPACING:
      gtk_wrap_box_set_vertical_spacing (box, g_value_get_uint (value));
      break;
    case PROP_HORIZONTAL_SPACING:
      gtk_wrap_box_set_horizontal_spacing (box, g_value_get_uint (value));
      break;
    case PROP_MINIMUM_LINE_CHILDREN:
      gtk_wrap_box_set_minimum_line_children (box, g_value_get_uint (value));
      break;
    case PROP_NATURAL_LINE_CHILDREN:
      gtk_wrap_box_set_natural_line_children (box, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*****************************************************
 *                 GtkWidgetClass                    *
 *****************************************************/

static gint
get_visible_children (GtkWrapBox  *box)
{
  GtkWrapBoxPrivate *priv = box->priv;
  GList             *list;
  gint               i = 0;

  for (list = priv->children; list; list = list->next)
    {
      GtkWrapBoxChild *child = list->data;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      i++;
    }

  return i;
}

static gint
get_visible_expand_children (GtkWrapBox     *box,
                             GtkOrientation  orientation,
                             GList          *cursor,
                             gint            n_visible)
{
  GList *list;
  gint   i, expand_children = 0;

  for (i = 0, list = cursor; (n_visible > 0 ? i < n_visible : TRUE) && list; list = list->next)
    {
      GtkWrapBoxChild *child = list->data;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      if ((orientation == GTK_ORIENTATION_HORIZONTAL && child->xexpand) ||
          (orientation == GTK_ORIENTATION_VERTICAL   && child->yexpand))
        expand_children++;

      i++;
    }

  return expand_children;
}

/* Used in columned modes where all items share at least their
 * equal widths or heights
 */
static void
get_average_item_size (GtkWrapBox      *box,
                       GtkOrientation   orientation,
                       gint            *min_size,
                       gint            *nat_size)
{
  GtkWrapBoxPrivate *priv = box->priv;
  GList             *list;
  gint               max_min_size = 0;
  gint               max_nat_size = 0;

  for (list = priv->children; list; list = list->next)
    {
      GtkWrapBoxChild *child = list->data;
      gint             child_min, child_nat;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_size_request_get_width (GTK_SIZE_REQUEST (child->widget),
                                      &child_min, &child_nat);

          child_min += child->xpadding * 2;
          child_nat += child->xpadding * 2;
        }
      else
        {
          gtk_size_request_get_height (GTK_SIZE_REQUEST (child->widget),
                                       &child_min, &child_nat);

          child_min += child->ypadding * 2;
          child_nat += child->ypadding * 2;
        }
      max_min_size = MAX (max_min_size, child_min);
      max_nat_size = MAX (max_nat_size, child_nat);
    }

  if (min_size)
    *min_size = max_min_size;

  if (nat_size)
    *nat_size = max_nat_size;
}


/* Gets the largest minimum/natural size for a given size
 * (used to get the largest item heights for a fixed item width and the opposite) */
static void
get_largest_size_for_opposing_orientation (GtkWrapBox         *box,
                                           GtkOrientation      orientation,
                                           gint                item_size,
                                           gint               *min_item_size,
                                           gint               *nat_item_size)
{
  GtkWrapBoxPrivate *priv = box->priv;
  GList             *list;
  gint               max_min_size = 0;
  gint               max_nat_size = 0;

  for (list = priv->children; list; list = list->next)
    {
      GtkWrapBoxChild *child = list->data;
      gint             child_min, child_nat;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_size_request_get_height_for_width (GTK_SIZE_REQUEST (child->widget),
                                                 item_size - child->xpadding * 2,
                                                 &child_min, &child_nat);
          child_min += child->ypadding * 2;
          child_nat += child->ypadding * 2;
        }
      else
        {
          gtk_size_request_get_width_for_height (GTK_SIZE_REQUEST (child->widget),
                                                 item_size - child->ypadding * 2,
                                                 &child_min, &child_nat);
          child_min += child->xpadding * 2;
          child_nat += child->xpadding * 2;
        }

      max_min_size = MAX (max_min_size, child_min);
      max_nat_size = MAX (max_nat_size, child_nat);
    }

  if (min_item_size)
    *min_item_size = max_min_size;

  if (nat_item_size)
    *nat_item_size = max_nat_size;
}


/* Gets the largest minimum/natural size on a single line for a given size
 * (used to get the largest line heights for a fixed item width and the opposite
 * while itterating over a list of children, note the new index is returned) */
static GList *
get_largest_size_for_line_in_opposing_orientation (GtkWrapBox      *box,
                                                   GtkOrientation   orientation,
                                                   GList           *cursor,
                                                   gint             line_length,
                                                   gint             item_size,
                                                   gint             extra_pixels,
                                                   gint            *min_item_size,
                                                   gint            *nat_item_size)
{
  GtkWrapBoxPrivate *priv   = box->priv;
  GList             *list;
  gint               max_min_size = 0;
  gint               max_nat_size = 0;
  gint               i;

  for (list = cursor, i = 0; list && i < line_length; list = list->next)
    {
      GtkWrapBoxChild *child = list->data;
      gint             child_min, child_nat, this_item_size;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      /* Distribute the extra pixels to the first children in the line
       * (could be fancier and spread them out more evenly) */
      this_item_size = item_size;
      if (extra_pixels > 0 &&
          priv->spreading == GTK_WRAP_BOX_SPREAD_EXPAND)
        {
          this_item_size++;
          extra_pixels--;
        }

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_size_request_get_height_for_width (GTK_SIZE_REQUEST (child->widget),
                                                 this_item_size - child->xpadding * 2,
                                                 &child_min, &child_nat);
          child_min += child->ypadding * 2;
          child_nat += child->ypadding * 2;
        }
      else
        {
          gtk_size_request_get_width_for_height (GTK_SIZE_REQUEST (child->widget),
                                                 this_item_size - child->ypadding * 2,
                                                 &child_min, &child_nat);
          child_min += child->xpadding * 2;
          child_nat += child->xpadding * 2;
        }

      max_min_size = MAX (max_min_size, child_min);
      max_nat_size = MAX (max_nat_size, child_nat);

      i++;
    }

  if (min_item_size)
    *min_item_size = max_min_size;

  if (nat_item_size)
    *nat_item_size = max_nat_size;

  /* Return next item in the list */
  return list;
}


/* Gets the largest minimum/natural size on a single line for a given allocated line size
 * (used to get the largest line heights for a width in pixels and the opposite
 * while itterating over a list of children, note the new index is returned) */
static GList *
get_largest_size_for_free_line_in_opposing_orientation (GtkWrapBox      *box,
                                                        GtkOrientation   orientation,
                                                        GList           *cursor,
                                                        gint             min_items,
                                                        gint             avail_size,
                                                        gint            *min_item_size,
                                                        gint            *nat_item_size,
                                                        gint            *extra_pixels,
                                                        GArray         **ret_array)
{
  GtkWrapBoxPrivate *priv = box->priv;
  GtkRequestedSize  *sizes;
  GList             *list;
  GArray            *array;
  gint               max_min_size = 0;
  gint               max_nat_size = 0;
  gint               i, size = avail_size;
  gint               line_length, spacing;
  gint               expand_children = 0;
  gint               expand_per_child;
  gint               expand_remainder;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    spacing = priv->horizontal_spacing;
  else
    spacing = priv->vertical_spacing;

  /* First determine the length of this line in items (how many items fit) */
  for (i = 0, list = cursor; size > 0 && list; list = list->next)
    {
      GtkWrapBoxChild *child = list->data;
      gint             child_size;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_size_request_get_width (GTK_SIZE_REQUEST (child->widget),
                                      NULL, &child_size);
          child_size += child->xpadding * 2;
        }
      else
        {
          gtk_size_request_get_height (GTK_SIZE_REQUEST (child->widget),
                                       NULL, &child_size);
          child_size += child->ypadding * 2;
        }

      if (i > 0)
        child_size += spacing;

      if (size - child_size >= 0)
        size -= child_size;
      else
        break;

      i++;
    }

  line_length = MAX (min_items, i);
  size        = avail_size;

  /* Collect the sizes of the items on this line */
  array = g_array_new (0, TRUE, sizeof (GtkRequestedSize));

  for (i = 0, list = cursor; i < line_length && list; list = list->next)
    {
      GtkWrapBoxChild  *child = list->data;
      GtkRequestedSize  requested;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      requested.data = child;
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_size_request_get_width (GTK_SIZE_REQUEST (child->widget),
                                      &requested.minimum_size,
                                      &requested.natural_size);

          size -= child->xpadding * 2;
        }
      else
        {
          gtk_size_request_get_height (GTK_SIZE_REQUEST (child->widget),
                                       &requested.minimum_size,
                                       &requested.natural_size);

          size -= child->ypadding * 2;
        }

      if (i > 0)
        size -= spacing;

      size -= requested.minimum_size;

      g_array_append_val (array, requested);

      i++;
    }

  sizes = (GtkRequestedSize *)array->data;
  size  = gtk_distribute_natural_allocation (size, array->len, sizes);

  if (extra_pixels)
    *extra_pixels = size;

  /* Cut out any expand space if we're not distributing any */
  if (priv->spreading != GTK_WRAP_BOX_SPREAD_EXPAND)
    size = 0;

  /* Count how many children are going to expand... */
  expand_children = get_visible_expand_children (box, orientation,
                                                 cursor, line_length);

  /* If no child prefers to expand, they all get some expand space */
  if (expand_children == 0)
    {
      expand_per_child = size / line_length;
      expand_remainder = size % line_length;
    }
  else
    {
      expand_per_child = size / expand_children;
      expand_remainder = size % expand_children;
    }

  /* Now add the remaining expand space and get the collective size of this line
   * in the opposing orientation */
  for (i = 0, list = cursor; i < line_length && list; list = list->next)
    {
      GtkWrapBoxChild *child = list->data;
      gint child_min, child_nat;

      if (!gtk_widget_get_visible (child->widget))
        continue;

      g_assert (child == sizes[i].data);

      if ((orientation == GTK_ORIENTATION_HORIZONTAL && child->xexpand) ||
          (orientation == GTK_ORIENTATION_VERTICAL   && child->yexpand) ||
          expand_children == 0)
        {
          sizes[i].minimum_size += expand_per_child;
          if (expand_remainder)
            {
              sizes[i].minimum_size++;
              expand_remainder--;
            }
        }

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_size_request_get_height_for_width (GTK_SIZE_REQUEST (child->widget),
                                                 sizes[i].minimum_size,
                                                 &child_min, &child_nat);
          child_min += child->ypadding * 2;
          child_nat += child->ypadding * 2;
        }
      else
        {
          gtk_size_request_get_width_for_height (GTK_SIZE_REQUEST (child->widget),
                                                 sizes[i].minimum_size,
                                                 &child_min, &child_nat);
          child_min += child->xpadding * 2;
          child_nat += child->xpadding * 2;
        }

      max_min_size = MAX (max_min_size, child_min);
      max_nat_size = MAX (max_nat_size, child_nat);

      i++;
    }

  if (ret_array)
    *ret_array = array;
  else
    g_array_free (array, TRUE);

  if (min_item_size)
    *min_item_size = max_min_size;

  if (nat_item_size)
    *nat_item_size = max_nat_size;

  /* Return the next item */
  return list;
}

static void
allocate_child (GtkWrapBox      *box,
                GtkWrapBoxChild *child,
                gint             item_offset,
                gint             line_offset,
                gint             item_size,
                gint             line_size)
{
  GtkWrapBoxPrivate  *priv   = box->priv;
  GtkAllocation       widget_allocation;
  GtkAllocation       child_allocation;
  GtkSizeRequestMode  request_mode;

  gtk_widget_get_allocation (GTK_WIDGET (box), &widget_allocation);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      child_allocation.x      = widget_allocation.x + item_offset + child->xpadding;
      child_allocation.y      = widget_allocation.y + line_offset + child->ypadding;
      child_allocation.width  = item_size - child->xpadding * 2;
      child_allocation.height = line_size - child->ypadding * 2;
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      child_allocation.x      = widget_allocation.x + line_offset + child->xpadding;
      child_allocation.y      = widget_allocation.y + item_offset + child->ypadding;
      child_allocation.width  = line_size - child->xpadding * 2;
      child_allocation.height = item_size - child->ypadding * 2;
    }

  request_mode = gtk_size_request_get_request_mode (GTK_SIZE_REQUEST (child->widget));
  if (!child->xfill)
    {
      gint width, height;

      if (!child->yfill && request_mode == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
        {
          gtk_size_request_get_height (GTK_SIZE_REQUEST (child->widget), NULL, &height);

          height = MIN (child_allocation.height, height);
        }
      else
        height = child_allocation.height;

      if (request_mode == GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT)
        gtk_size_request_get_width_for_height (GTK_SIZE_REQUEST (child->widget),
                                               height, NULL, &width);
      else
        gtk_size_request_get_width (GTK_SIZE_REQUEST (child->widget), NULL, &width);

      width = MIN (child_allocation.width, width);
      child_allocation.x     = child_allocation.x + (child_allocation.width - width) / 2;
      child_allocation.width = width;
    }

  if (!child->yfill)
    {
      gint height;

      /* Note here child_allocation.width is already changed if (!child->xfill) */
      if (request_mode == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
        gtk_size_request_get_height_for_width (GTK_SIZE_REQUEST (child->widget),
                                               child_allocation.width, NULL, &height);
      else
        gtk_size_request_get_height (GTK_SIZE_REQUEST (child->widget), NULL, &height);

      height = MIN (child_allocation.height, height);
      child_allocation.y      = child_allocation.y + (child_allocation.height - height) / 2;
      child_allocation.height = height;
    }

  gtk_widget_size_allocate (child->widget, &child_allocation);
}


typedef struct {
  GArray *requested;
  gint    extra_pixels;
} AllocatedLine;

static void
gtk_wrap_box_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  GtkWrapBox         *box  = GTK_WRAP_BOX (widget);
  GtkWrapBoxPrivate  *priv = box->priv;
  GtkRequestedSize   *sizes = NULL;
  GArray             *array;
  guint               border_width;
  gint                avail_size, avail_other_size, min_items, item_spacing, line_spacing;

  gtk_widget_set_allocation (widget, allocation);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  min_items    = MAX (1, priv->minimum_line_children);

  /* Collect the line sizes for GTK_WRAP_ALLOCATE_FREE and
   * GTK_WRAP_ALLOCATE_ALIGNED modes */
  array = g_array_new (0, TRUE, sizeof (GtkRequestedSize));

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      avail_size       = allocation->width  - border_width * 2;
      avail_other_size = allocation->height - border_width * 2;
      item_spacing     = priv->horizontal_spacing;
      line_spacing     = priv->vertical_spacing;
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      avail_size       = allocation->height - border_width * 2;
      avail_other_size = allocation->width  - border_width * 2;
      item_spacing     = priv->vertical_spacing;
      line_spacing     = priv->horizontal_spacing;
    }


  if (priv->mode == GTK_WRAP_ALLOCATE_ALIGNED ||
      priv->mode == GTK_WRAP_ALLOCATE_HOMOGENEOUS)
    {
      GList *list;
      gint   min_item_size, nat_item_size;
      gint   line_length;
      gint   item_size;
      gint   line_size = 0, min_fixed_line_size = 0, nat_fixed_line_size = 0;
      gint   line_offset, n_children, n_lines, line_count;
      gint   extra_pixels, extra_per_item, extra_extra;
      gint   i;

      get_average_item_size (box, priv->orientation, &min_item_size, &nat_item_size);

      /* By default wrap at the natural item width */
      line_length = avail_size / (nat_item_size + item_spacing);

      /* After the above aproximation, check if we cant fit one more on the line */
      if (line_length * item_spacing + (line_length + 1) * nat_item_size <= avail_size)
        line_length++;

      /* Its possible we were allocated just less than the natural width of the
       * minimum item wrap length */
      line_length = MAX (min_items, line_length);

      /* Now we need the real item allocation size */
      item_size = (avail_size - (line_length - 1) * item_spacing) / line_length;

      /* Cut out the expand space if we're not distributing any */
      if (priv->spreading != GTK_WRAP_BOX_SPREAD_EXPAND)
        item_size = MIN (item_size, nat_item_size);

      /* Get the real extra pixels incase of GTK_WRAP_BOX_SPREAD_BEGIN lines */
      extra_pixels   = avail_size - (line_length - 1) * item_spacing - item_size * line_length;
      extra_per_item = extra_pixels / MAX (line_length -1, 1);
      extra_extra    = extra_pixels % MAX (line_length -1, 1);

      /* Get how many lines that wraps to */
      n_children = get_visible_children (box);
      n_lines    = n_children / line_length;
      if ((n_children % line_length) > 0)
        n_lines++;

      n_lines = MAX (n_lines, 1);

      /* Here we just use the largest height-for-width and use that for the height
       * of all lines */
      if (priv->mode == GTK_WRAP_ALLOCATE_HOMOGENEOUS)
        {
          get_largest_size_for_opposing_orientation (box, priv->orientation, item_size,
                                                     &min_fixed_line_size,
                                                     &nat_fixed_line_size);

          /* resolve a fixed 'line_size' */
          line_size = (avail_other_size - (n_lines - 1) * line_spacing) / n_lines;
          line_size = MIN (line_size, nat_fixed_line_size);
        }
      else /* GTK_WRAP_ALLOCATE_ALIGNED */
        {
          /* spread out the available size in the opposing orientation into an array of
           * lines (and then allocate those lines naturally) */
          GList *list;
          gboolean first_line = TRUE;

          /* In ALIGNED mode, all items have the same size in the box's orientation except
           * individual lines may have a different size */
          for (i = 0, list = priv->children; list != NULL; i++)
            {
              GtkRequestedSize    requested;

              list =
                get_largest_size_for_line_in_opposing_orientation (box, priv->orientation,
                                                                   list, line_length,
                                                                   item_size, extra_pixels,
                                                                   &requested.minimum_size,
                                                                   &requested.natural_size);


              /* Its possible a line is made of completely invisible children */
              if (requested.natural_size > 0)
                {
                  if (first_line)
                    first_line = FALSE;
                  else
                    avail_other_size -= line_spacing;

                  avail_other_size -= requested.minimum_size;

                  requested.data = GINT_TO_POINTER (i);
                  g_array_append_val (array, requested);
                }
            }

          /* Distribute space among lines naturally */
          sizes            = (GtkRequestedSize *)array->data;
          avail_other_size = gtk_distribute_natural_allocation (avail_other_size, array->len, sizes);
        }

      line_offset = border_width;

      for (i = 0, line_count = 0, list = priv->children; list; list = list->next)
        {
          GtkWrapBoxChild *child = list->data;
          gint                position, this_line_size, item_offset;
          gint                this_item_size;

          if (!gtk_widget_get_visible (child->widget))
            continue;

          /* Get item position */
          position = i % line_length;

          /* adjust the line_offset/count at the beginning of each new line */
          if (i > 0 && position == 0)
            {
              if (priv->mode == GTK_WRAP_ALLOCATE_HOMOGENEOUS)
                line_offset += line_size + line_spacing;
              else /* aligned mode */
                line_offset += sizes[line_count].minimum_size + line_spacing;

              line_count++;
            }

          /* We could be smarter here and distribute the extra pixels more
           * evenly across the children */
          if (position < extra_pixels && priv->spreading == GTK_WRAP_BOX_SPREAD_EXPAND)
            this_item_size = item_size + 1;
          else
            this_item_size = item_size;

          /* Push the index along for the last line when spreading to the end */
          if (priv->spreading == GTK_WRAP_BOX_SPREAD_END &&
              line_count == n_lines -1)
            {
              gint extra_items = n_children % line_length;

              position += line_length - extra_items;
            }

          item_offset = border_width + (position * item_size) + (position * item_spacing);

          if (priv->spreading == GTK_WRAP_BOX_SPREAD_EVEN)
            {
              item_offset += position * extra_per_item;
              item_offset += MIN (position, extra_extra);
            }
          else if (priv->spreading == GTK_WRAP_BOX_SPREAD_END)
            item_offset += extra_pixels;
          else if (priv->spreading == GTK_WRAP_BOX_SPREAD_EXPAND)
            item_offset += MIN (position, extra_pixels);

          /* Get the allocation size for this line */
          if (priv->mode == GTK_WRAP_ALLOCATE_HOMOGENEOUS)
            this_line_size = line_size;
          else
            this_line_size = sizes[line_count].minimum_size;

          /* Do the actual allocation */
          allocate_child (box, child, item_offset, line_offset, this_item_size, this_line_size);

          i++;
        }
    }
  else /* GTK_WRAP_ALLOCATE_FREE */
    {
      /* Here we just fit as many children as we can allocate their natural size to
       * on each line and add the heights for each of them on each line */
      GtkRequestedSize requested;
      GList           *list = priv->children;
      gboolean         first_line = TRUE;
      gint             i, line_count = 0;
      gint             line_offset, item_offset;
      gint             extra_pixels;

      while (list != NULL)
        {
          GArray         *line_array;
          AllocatedLine  *line;

          list =
            get_largest_size_for_free_line_in_opposing_orientation (box, priv->orientation,
                                                                    list, min_items, avail_size,
                                                                    &requested.minimum_size,
                                                                    &requested.natural_size,
                                                                    &extra_pixels,
                                                                    &line_array);

          /* Its possible a line is made of completely invisible children */
          if (requested.natural_size > 0)
            {
              if (first_line)
                first_line = FALSE;
              else
                avail_other_size -= line_spacing;

              avail_other_size -= requested.minimum_size;

              line = g_slice_new0 (AllocatedLine);
              line->requested    = line_array;
              line->extra_pixels = extra_pixels;

              requested.data  = line;

              g_array_append_val (array, requested);
            }
        }

      /* Distribute space among lines naturally, dont give lines expand space just let them
       * unwrap/wrap in and out of the allocated extra space */
      sizes            = (GtkRequestedSize *)array->data;
      avail_other_size = gtk_distribute_natural_allocation (avail_other_size, array->len, sizes);

      for (line_offset = border_width, line_count = 0; line_count < array->len; line_count++)
        {
          AllocatedLine    *line       = (AllocatedLine *)sizes[line_count].data;
          GArray           *line_array = line->requested;
          GtkRequestedSize *line_sizes = (GtkRequestedSize *)line_array->data;
          gint              line_size  = sizes[line_count].minimum_size;
          gint              extra_per_item = 0;
          gint              extra_extra = 0;

          /* Set line start offset */
          item_offset = border_width;

          if (priv->spreading == GTK_WRAP_BOX_SPREAD_END)
            item_offset += line->extra_pixels;
          else if (priv->spreading == GTK_WRAP_BOX_SPREAD_EVEN)
            {
              extra_per_item = line->extra_pixels / MAX (line_array->len -1, 1);
              extra_extra    = line->extra_pixels % MAX (line_array->len -1, 1);
            }

          for (i = 0; i < line_array->len; i++)
            {
              GtkWrapBoxChild *child     = line_sizes[i].data;
              gint                item_size = line_sizes[i].minimum_size;

              item_size += (priv->orientation == GTK_ORIENTATION_HORIZONTAL) ?
                child->xpadding * 2 : child->ypadding * 2;

              /* Do the actual allocation */
              allocate_child (box, child, item_offset, line_offset, item_size, line_size);

              /* Add extra space evenly between children */
              if (priv->spreading == GTK_WRAP_BOX_SPREAD_EVEN)
                {
                  item_offset += extra_per_item;
                  if (i < extra_extra)
                    item_offset++;
                }

              /* Move item cursor along for the next allocation */
              item_offset += item_spacing;
              item_offset += item_size;
            }

          /* New line, increment offset and reset item cursor */
          line_offset += line_spacing;
          line_offset += line_size;

          /* Free the array for this line now its not needed anymore */
          g_array_free (line_array, TRUE);
          g_slice_free (AllocatedLine, line);
        }
    }

  g_array_free (array, TRUE);
}

/*****************************************************
 *                GtkContainerClass                  *
 *****************************************************/
static void
gtk_wrap_box_add (GtkContainer *container,
                     GtkWidget    *widget)
{
  gtk_wrap_box_insert_child (GTK_WRAP_BOX (container), widget, -1,
                             0, 0, FALSE, FALSE, FALSE, FALSE);
}

static gint
find_child_in_list (GtkWrapBoxChild *child_in_list,
                    GtkWidget       *search)
{
  return (child_in_list->widget == search) ? 0 : -1;
}

static void
gtk_wrap_box_remove (GtkContainer *container,
                     GtkWidget    *widget)
{
  GtkWrapBox        *box = GTK_WRAP_BOX (container);
  GtkWrapBoxPrivate *priv   = box->priv;
  GList             *list;

  list = g_list_find_custom (priv->children, widget,
                             (GCompareFunc)find_child_in_list);

  if (list)
    {
      GtkWrapBoxChild *child = list->data;
      gboolean was_visible = gtk_widget_get_visible (widget);

      gtk_widget_unparent (widget);

      g_slice_free (GtkWrapBoxChild, child);
      priv->children = g_list_delete_link (priv->children, list);

      if (was_visible && gtk_widget_get_visible (GTK_WIDGET (container)))
        gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
gtk_wrap_box_forall (GtkContainer *container,
                     gboolean      include_internals,
                     GtkCallback   callback,
                     gpointer      callback_data)
{
  GtkWrapBox        *box = GTK_WRAP_BOX (container);
  GtkWrapBoxPrivate *priv   = box->priv;
  GtkWrapBoxChild   *child;
  GList             *list;

  for (list = priv->children; list; list = list->next)
    {
      child = list->data;

      (* callback) (child->widget, callback_data);
    }
}

static GType
gtk_wrap_box_child_type (GtkContainer   *container)
{
  return GTK_TYPE_WIDGET;
}

static void
gtk_wrap_box_set_child_property (GtkContainer    *container,
                                 GtkWidget       *widget,
                                 guint            property_id,
                                 const GValue    *value,
                                 GParamSpec      *pspec)
{
  GtkWrapBox        *box = GTK_WRAP_BOX (container);
  GtkWrapBoxPrivate *priv   = box->priv;
  GtkWrapBoxChild   *child;
  GList             *list;

  list = g_list_find_custom (priv->children, widget,
                             (GCompareFunc)find_child_in_list);
  g_return_if_fail (list != NULL);

  child = list->data;

  switch (property_id)
    {
    case CHILD_PROP_X_EXPAND:
      child->xexpand = g_value_get_boolean (value);
      break;
    case CHILD_PROP_X_FILL:
      child->xfill = g_value_get_boolean (value);
      break;
    case CHILD_PROP_Y_EXPAND:
      child->yexpand = g_value_get_boolean (value);
      break;
    case CHILD_PROP_Y_FILL:
      child->yfill = g_value_get_boolean (value);
      break;
    case CHILD_PROP_X_PADDING:
      child->xpadding = g_value_get_uint (value);
      break;
    case CHILD_PROP_Y_PADDING:
      child->ypadding = g_value_get_uint (value);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }

  if (gtk_widget_get_visible (widget) &&
      gtk_widget_get_visible (GTK_WIDGET (box)))
    gtk_widget_queue_resize (widget);
}

static void
gtk_wrap_box_get_child_property (GtkContainer    *container,
                                 GtkWidget       *widget,
                                 guint            property_id,
                                 GValue          *value,
                                 GParamSpec      *pspec)
{
  GtkWrapBox        *box = GTK_WRAP_BOX (container);
  GtkWrapBoxPrivate *priv   = box->priv;
  GtkWrapBoxChild   *child;
  GList             *list;

  list = g_list_find_custom (priv->children, widget,
                             (GCompareFunc)find_child_in_list);
  g_return_if_fail (list != NULL);

  child = list->data;

  switch (property_id)
    {
    case CHILD_PROP_X_EXPAND:
      g_value_set_boolean (value, child->xexpand);
      break;
    case CHILD_PROP_X_FILL:
      g_value_set_boolean (value, child->xfill);
      break;
    case CHILD_PROP_Y_EXPAND:
      g_value_set_boolean (value, child->yexpand);
      break;
    case CHILD_PROP_Y_FILL:
      g_value_set_boolean (value, child->yfill);
      break;
    case CHILD_PROP_X_PADDING:
      g_value_set_uint (value, child->xpadding);
      break;
    case CHILD_PROP_Y_PADDING:
      g_value_set_uint (value, child->ypadding);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

/*****************************************************
 *               GtkSizeRequestIface                 *
 *****************************************************/

static void
gtk_wrap_box_size_request_init (GtkSizeRequestIface *iface)
{
  iface->get_request_mode     = gtk_wrap_box_get_request_mode;
  iface->get_width            = gtk_wrap_box_get_width;
  iface->get_height           = gtk_wrap_box_get_height;
  iface->get_height_for_width = gtk_wrap_box_get_height_for_width;
  iface->get_width_for_height = gtk_wrap_box_get_width_for_height;
}

static GtkSizeRequestMode
gtk_wrap_box_get_request_mode (GtkSizeRequest *widget)
{
  GtkWrapBox        *box = GTK_WRAP_BOX (widget);
  GtkWrapBoxPrivate *priv   = box->priv;

  return (priv->orientation == GTK_ORIENTATION_HORIZONTAL) ?
    GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH : GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

/* Gets the largest minimum and natural length of
 * 'line_length' consecutive items */
static void
get_largest_line_length (GtkWrapBox      *box,
                         GtkOrientation   orientation,
                         gint             line_length,
                         gint            *min_size,
                         gint            *nat_size)
{
  GtkWrapBoxPrivate *priv = box->priv;
  GList             *list;
  gint               max_min_size = 0;
  gint               max_nat_size = 0;
  gint               spacing;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    spacing = priv->horizontal_spacing;
  else
    spacing = priv->vertical_spacing;

  /* Get the largest size of 'line_length' consecutive items in the list.
   */
  for (list = priv->children; list; list = list->next)
    {
      GList *l;
      gint   line_min = 0;
      gint   line_nat = 0;
      gint   i;

      for (l = list, i = 0; l && i < line_length; l = l->next)
        {
          GtkWrapBoxChild *child = list->data;
          gint             child_min, child_nat;

          if (!gtk_widget_get_visible (child->widget))
            continue;

          if (orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              gtk_size_request_get_width (GTK_SIZE_REQUEST (child->widget),
                                          &child_min, &child_nat);

              child_min += child->xpadding * 2;
              child_nat += child->xpadding * 2;
            }
          else /* GTK_ORIENTATION_VERTICAL */
            {
              gtk_size_request_get_height (GTK_SIZE_REQUEST (child->widget),
                                           &child_min, &child_nat);

              child_min += child->ypadding * 2;
              child_nat += child->ypadding * 2;
            }

          if (i > 0)
            {
              line_min += spacing;
              line_nat += spacing;
            }

          line_min += child_min;
          line_nat += child_nat;

          i++;
        }

      max_min_size = MAX (max_min_size, line_min);
      max_nat_size = MAX (max_nat_size, line_nat);
    }

  if (min_size)
    *min_size = max_min_size;

  if (nat_size)
    *nat_size = max_nat_size;
}


static void
gtk_wrap_box_get_width (GtkSizeRequest      *widget,
                        gint                *minimum_size,
                        gint                *natural_size)
{
  GtkWrapBox        *box  = GTK_WRAP_BOX (widget);
  GtkWrapBoxPrivate *priv = box->priv;
  guint              border_width;
  gint               min_item_width, nat_item_width;
  gint               min_items, nat_items;
  gint               min_width, nat_width;

  min_items = MAX (1,         priv->minimum_line_children);
  nat_items = MAX (min_items, priv->natural_line_children);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
      min_width    = nat_width = border_width * 2;

      if (priv->mode == GTK_WRAP_ALLOCATE_FREE)
        {
          if (priv->minimum_line_children <= 1)
            {
              get_average_item_size (box, GTK_ORIENTATION_HORIZONTAL,
                                     &min_item_width, &nat_item_width);

              min_width += min_item_width;
              nat_width += nat_item_width;
            }
          else
            {
              gint min_line_length, nat_line_length;

              get_largest_line_length (box, GTK_ORIENTATION_HORIZONTAL, min_items,
                                       &min_line_length, &nat_line_length);

              if (nat_items > min_items)
                get_largest_line_length (box, GTK_ORIENTATION_HORIZONTAL, nat_items,
                                         NULL, &nat_line_length);

              min_width += min_line_length;
              nat_width += nat_line_length;
            }
        }
      else /* In ALIGNED or HOMOGENEOUS modes; horizontally oriented boxs
            * give the same width to all children */
        {
          get_average_item_size (box, GTK_ORIENTATION_HORIZONTAL,
                                 &min_item_width, &nat_item_width);

          min_width += min_item_width * min_items;
          min_width += (min_items -1) * priv->horizontal_spacing;

          nat_width += nat_item_width * nat_items;
          nat_width += (nat_items -1) * priv->horizontal_spacing;
        }
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      /* Return the width for the minimum height */
      gint min_height;

      gtk_size_request_get_height (widget, &min_height, NULL);
      gtk_size_request_get_width_for_height (widget, min_height, &min_width, &nat_width);

    }

  if (minimum_size)
    *minimum_size = min_width;

  if (natural_size)
    *natural_size = nat_width;
}

static void
gtk_wrap_box_get_height (GtkSizeRequest      *widget,
                         gint                *minimum_size,
                         gint                *natural_size)
{
  GtkWrapBox        *box  = GTK_WRAP_BOX (widget);
  GtkWrapBoxPrivate *priv = box->priv;
  guint              border_width;
  gint               min_item_height, nat_item_height;
  gint               min_items, nat_items;
  gint               min_height, nat_height;

  min_items = MAX (1,         priv->minimum_line_children);
  nat_items = MAX (min_items, priv->natural_line_children);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* Return the height for the minimum width */
      gint min_width;

      gtk_size_request_get_width (widget, &min_width, NULL);
      gtk_size_request_get_height_for_width (widget, min_width, &min_height, &nat_height);
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
      min_height   = nat_height = border_width * 2;

      if (priv->mode == GTK_WRAP_ALLOCATE_FREE)
        {
          if (priv->minimum_line_children <= 1)
            {
              get_average_item_size (box, GTK_ORIENTATION_VERTICAL,
                                     &min_item_height, &nat_item_height);

              min_height += min_item_height;
              nat_height += nat_item_height;
            }
          else
            {
              gint min_line_length, nat_line_length;

              get_largest_line_length (box, GTK_ORIENTATION_VERTICAL, min_items,
                                       &min_line_length, &nat_line_length);

              if (nat_items > min_items)
                get_largest_line_length (box, GTK_ORIENTATION_VERTICAL, nat_items,
                                         NULL, &nat_line_length);

              min_height += min_line_length;
              nat_height += nat_line_length;
            }
        }
      else /* In ALIGNED or HOMOGENEOUS modes; horizontally oriented boxs
            * give the same width to all children */
        {
          get_average_item_size (box, GTK_ORIENTATION_VERTICAL,
                                 &min_item_height, &nat_item_height);

          min_height += min_item_height * min_items;
          min_height += (min_items -1) * priv->vertical_spacing;

          nat_height += nat_item_height * nat_items;
          nat_height += (nat_items -1) * priv->vertical_spacing;
        }
    }

  if (minimum_size)
    *minimum_size = min_height;

  if (natural_size)
    *natural_size = nat_height;
}

static void
gtk_wrap_box_get_height_for_width (GtkSizeRequest      *widget,
                                   gint                 width,
                                   gint                *minimum_height,
                                   gint                *natural_height)
{
  GtkWrapBox        *box = GTK_WRAP_BOX (widget);
  GtkWrapBoxPrivate *priv   = box->priv;
  guint              border_width;
  gint               min_item_width, nat_item_width;
  gint               min_items;
  gint               min_height, nat_height;
  gint               avail_size;

  min_items = MAX (1, priv->minimum_line_children);

  min_height = 0;
  nat_height = 0;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gint min_width;

      /* Make sure its no smaller than the minimum */
      gtk_size_request_get_width (widget, &min_width, NULL);

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

      avail_size  = MAX (width, min_width);
      avail_size -= border_width * 2;

      if (priv->mode == GTK_WRAP_ALLOCATE_ALIGNED ||
          priv->mode == GTK_WRAP_ALLOCATE_HOMOGENEOUS)
        {
          gint line_length;
          gint item_size, extra_pixels;

          get_average_item_size (box, GTK_ORIENTATION_HORIZONTAL, &min_item_width, &nat_item_width);

          /* By default wrap at the natural item width */
          line_length = avail_size / (nat_item_width + priv->horizontal_spacing);

          /* After the above aproximation, check if we cant fit one more on the line */
          if (line_length * priv->horizontal_spacing + (line_length + 1) * nat_item_width <= avail_size)
            line_length++;

          /* Its possible we were allocated just less than the natural width of the
           * minimum item wrap length */
          line_length = MAX (min_items, line_length);

          /* Now we need the real item allocation size */
          item_size = (avail_size - (line_length - 1) * priv->horizontal_spacing) / line_length;

          /* Cut out the expand space if we're not distributing any */
          if (priv->spreading != GTK_WRAP_BOX_SPREAD_EXPAND)
            {
              item_size    = MIN (item_size, nat_item_width);
              extra_pixels = 0;
            }
          else
            /* Collect the extra pixels for expand children */
            extra_pixels = (avail_size - (line_length - 1) * priv->horizontal_spacing) % line_length;

          if (priv->mode == GTK_WRAP_ALLOCATE_HOMOGENEOUS)
            {
              gint min_item_height, nat_item_height;
              gint lines, n_children;

              /* Here we just use the largest height-for-width and
               * add up the size accordingly */
              get_largest_size_for_opposing_orientation (box, GTK_ORIENTATION_HORIZONTAL, item_size,
                                                         &min_item_height, &nat_item_height);

              /* Round up how many lines we need to allocate for */
              n_children = get_visible_children (box);
              lines      = n_children / line_length;
              if ((n_children % line_length) > 0)
                lines++;

              min_height = min_item_height * lines;
              nat_height = nat_item_height * lines;

              min_height += (lines - 1) * priv->vertical_spacing;
              nat_height += (lines - 1) * priv->vertical_spacing;
            }
          else /* GTK_WRAP_ALLOCATE_ALIGNED */
            {
              GList *list = priv->children;
              gint min_line_height, nat_line_height;
              gboolean first_line = TRUE;

              /* In ALIGNED mode, all items have the same size in the box's orientation except
               * individual rows may have a different size */
              while (list != NULL)
                {
                  list =
                    get_largest_size_for_line_in_opposing_orientation (box, GTK_ORIENTATION_HORIZONTAL,
                                                                       list, line_length,
                                                                       item_size, extra_pixels,
                                                                       &min_line_height, &nat_line_height);


                  /* Its possible the line only had invisible widgets */
                  if (nat_line_height > 0)
                    {
                      if (first_line)
                        first_line = FALSE;
                      else
                        {
                          min_height += priv->vertical_spacing;
                          nat_height += priv->vertical_spacing;
                        }

                      min_height += min_line_height;
                      nat_height += nat_line_height;
                    }
                }
            }
        }
      else /* GTK_WRAP_ALLOCATE_FREE */
        {
          /* Here we just fit as many children as we can allocate their natural size to
           * on each line and add the heights for each of them on each line */
          GList *list = priv->children;
          gint min_line_height = 0, nat_line_height = 0;
          gboolean first_line = TRUE;

          while (list != NULL)
            {
              list =
                get_largest_size_for_free_line_in_opposing_orientation (box, GTK_ORIENTATION_HORIZONTAL,
                                                                        list, min_items, avail_size,
                                                                        &min_line_height, &nat_line_height,
                                                                        NULL, NULL);

              /* Its possible the last line only had invisible widgets */
              if (nat_line_height > 0)
                {
                  if (first_line)
                    first_line = FALSE;
                  else
                    {
                      min_height += priv->vertical_spacing;
                      nat_height += priv->vertical_spacing;
                    }

                  min_height += min_line_height;
                  nat_height += nat_line_height;
                }
            }
        }

      min_height += border_width * 2;
      nat_height += border_width * 2;
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      /* Return the minimum height */
      gtk_size_request_get_height (widget, &min_height, &nat_height);
    }

  if (minimum_height)
    *minimum_height = min_height;

  if (natural_height)
    *natural_height = nat_height;
}

static void
gtk_wrap_box_get_width_for_height (GtkSizeRequest      *widget,
                                   gint                 height,
                                   gint                *minimum_width,
                                   gint                *natural_width)
{
  GtkWrapBox        *box = GTK_WRAP_BOX (widget);
  GtkWrapBoxPrivate *priv   = box->priv;
  guint              border_width;
  gint               min_item_height, nat_item_height;
  gint               min_items;
  gint               min_width, nat_width;
  gint               avail_size;

  min_items = MAX (1, priv->minimum_line_children);

  min_width = 0;
  nat_width = 0;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* Return the minimum width */
      gtk_size_request_get_width (widget, &min_width, &nat_width);
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      gint min_height;

      /* Make sure its no smaller than the minimum */
      gtk_size_request_get_height (widget, &min_height, NULL);

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

      avail_size  = MAX (height, min_height);
      avail_size -= border_width * 2;

      if (priv->mode == GTK_WRAP_ALLOCATE_ALIGNED ||
          priv->mode == GTK_WRAP_ALLOCATE_HOMOGENEOUS)
        {
          gint line_length;
          gint item_size, extra_pixels;

          get_average_item_size (box, GTK_ORIENTATION_VERTICAL, &min_item_height, &nat_item_height);

          /* By default wrap at the natural item width */
          line_length = avail_size / (nat_item_height + priv->vertical_spacing);

          /* After the above aproximation, check if we cant fit one more on the line */
          if (line_length * priv->vertical_spacing + (line_length + 1) * nat_item_height <= avail_size)
            line_length++;

          /* Its possible we were allocated just less than the natural width of the
           * minimum item wrap length */
          line_length = MAX (min_items, line_length);

          /* Now we need the real item allocation size */
          item_size = (avail_size - (line_length - 1) * priv->vertical_spacing) / line_length;

          /* Cut out the expand space if we're not distributing any */
          if (priv->spreading != GTK_WRAP_BOX_SPREAD_EXPAND)
            {
              item_size    = MIN (item_size, nat_item_height);
              extra_pixels = 0;
            }
          else
            /* Collect the extra pixels for expand children */
            extra_pixels = (avail_size - (line_length - 1) * priv->vertical_spacing) % line_length;

          if (priv->mode == GTK_WRAP_ALLOCATE_HOMOGENEOUS)
            {
              gint min_item_width, nat_item_width;
              gint lines, n_children;

              /* Here we just use the largest height-for-width and
               * add up the size accordingly */
              get_largest_size_for_opposing_orientation (box, GTK_ORIENTATION_VERTICAL, item_size,
                                                         &min_item_width, &nat_item_width);

              /* Round up how many lines we need to allocate for */
              n_children = get_visible_children (box);
              lines      = n_children / line_length;
              if ((n_children % line_length) > 0)
                lines++;

              min_width = min_item_width * lines;
              nat_width = nat_item_width * lines;

              min_width += (lines - 1) * priv->horizontal_spacing;
              nat_width += (lines - 1) * priv->horizontal_spacing;
            }
          else /* GTK_WRAP_ALLOCATE_ALIGNED */
            {
              GList *list = priv->children;
              gint min_line_width, nat_line_width;
              gboolean first_line = TRUE;

              /* In ALIGNED mode, all items have the same size in the box's orientation except
               * individual rows may have a different size */
              while (list != NULL)
                {
                  list =
                    get_largest_size_for_line_in_opposing_orientation (box, GTK_ORIENTATION_VERTICAL,
                                                                       list, line_length,
                                                                       item_size, extra_pixels,
                                                                       &min_line_width, &nat_line_width);

                  /* Its possible the last line only had invisible widgets */
                  if (nat_line_width > 0)
                    {
                      if (first_line)
                        first_line = FALSE;
                      else
                        {
                          min_width += priv->horizontal_spacing;
                          nat_width += priv->horizontal_spacing;
                        }

                      min_width += min_line_width;
                      nat_width += nat_line_width;
                    }
                }
            }
        }
      else /* GTK_WRAP_ALLOCATE_FREE */
        {
          /* Here we just fit as many children as we can allocate their natural size to
           * on each line and add the heights for each of them on each line */
          GList *list = priv->children;
          gint min_line_width = 0, nat_line_width = 0;
          gboolean first_line = TRUE;

          while (list != NULL)
            {
              list =
                get_largest_size_for_free_line_in_opposing_orientation (box, GTK_ORIENTATION_VERTICAL,
                                                                        list, min_items, avail_size,
                                                                        &min_line_width, &nat_line_width,
                                                                        NULL, NULL);

              /* Its possible the last line only had invisible widgets */
              if (nat_line_width > 0)
                {
                  if (first_line)
                    first_line = FALSE;
                  else
                    {
                      min_width += priv->horizontal_spacing;
                      nat_width += priv->horizontal_spacing;
                    }

                  min_width += min_line_width;
                  nat_width += nat_line_width;
                }
            }
        }

      min_width += border_width * 2;
      nat_width += border_width * 2;
    }

  if (minimum_width)
    *minimum_width = min_width;

  if (natural_width)
    *natural_width = nat_width;
}

/*****************************************************
 *                       API                         *
 *****************************************************/

/**
 * gtk_wrap_box_new:
 * @allocation_mode: The #GtkWrapAllocationMode to use
 * @spreading: The #GtkWrapBoxSpreading policy to use
 * @horizontal_spacing: The horizontal spacing to add between children
 * @vertical_spacing: The vertical spacing to add between children
 *
 * Creates an #GtkWrapBox.
 *
 * Returns: A new #GtkWrapBox container
 */
GtkWidget *
gtk_wrap_box_new (GtkWrapAllocationMode mode,
                  GtkWrapBoxSpreading   spreading,
                  guint                 horizontal_spacing,
                  guint                 vertical_spacing)
{
  return (GtkWidget *)g_object_new (GTK_TYPE_WRAP_BOX,
                                    "allocation-mode", mode,
                                    "spreading", spreading,
                                    "vertical-spacing", vertical_spacing,
                                    "horizontal-spacing", horizontal_spacing,
                                    NULL);
}

/**
 * gtk_wrap_box_set_allocation_mode:
 * @box: An #GtkWrapBox
 * @mode: The #GtkWrapAllocationMode to use.
 *
 * Sets the allocation mode for @box's children.
 */
void
gtk_wrap_box_set_allocation_mode (GtkWrapBox           *box,
                                  GtkWrapAllocationMode mode)
{
  GtkWrapBoxPrivate *priv;

  g_return_if_fail (GTK_IS_WRAP_BOX (box));

  priv = box->priv;

  if (priv->mode != mode)
    {
      priv->mode = mode;

      gtk_widget_queue_resize (GTK_WIDGET (box));

      g_object_notify (G_OBJECT (box), "allocation-mode");
    }
}

/**
 * gtk_wrap_box_get_allocation_mode:
 * @box: An #GtkWrapBox
 *
 * Gets the allocation mode.
 *
 * Returns: The #GtkWrapAllocationMode for @box.
 */
GtkWrapAllocationMode
gtk_wrap_box_get_allocation_mode (GtkWrapBox *box)
{
  g_return_val_if_fail (GTK_IS_WRAP_BOX (box), FALSE);

  return box->priv->mode;
}


/**
 * gtk_wrap_box_set_spreading:
 * @box: An #GtkWrapBox
 * @spreading: The #GtkWrapBoxSpreading to use.
 *
 * Sets the spreading mode for @box's children.
 */
void
gtk_wrap_box_set_spreading (GtkWrapBox          *box,
                            GtkWrapBoxSpreading  spreading)
{
  GtkWrapBoxPrivate *priv;

  g_return_if_fail (GTK_IS_WRAP_BOX (box));

  priv = box->priv;

  if (priv->spreading != spreading)
    {
      priv->spreading = spreading;

      gtk_widget_queue_resize (GTK_WIDGET (box));

      g_object_notify (G_OBJECT (box), "spreading");
    }
}

/**
 * gtk_wrap_box_get_spreading:
 * @box: An #GtkWrapBox
 *
 * Gets the spreading mode.
 *
 * Returns: The #GtkWrapBoxSpreading for @box.
 */
GtkWrapBoxSpreading
gtk_wrap_box_get_spreading (GtkWrapBox *box)
{
  g_return_val_if_fail (GTK_IS_WRAP_BOX (box), FALSE);

  return box->priv->spreading;
}


/**
 * gtk_wrap_box_set_vertical_spacing:
 * @box: An #GtkWrapBox
 * @spacing: The spacing to use.
 *
 * Sets the vertical space to add between children.
 */
void
gtk_wrap_box_set_vertical_spacing  (GtkWrapBox    *box,
                                    guint          spacing)
{
  GtkWrapBoxPrivate *priv;

  g_return_if_fail (GTK_IS_WRAP_BOX (box));

  priv = box->priv;

  if (priv->vertical_spacing != spacing)
    {
      priv->vertical_spacing = spacing;

      gtk_widget_queue_resize (GTK_WIDGET (box));

      g_object_notify (G_OBJECT (box), "vertical-spacing");
    }
}

/**
 * gtk_wrap_box_get_vertical_spacing:
 * @box: An #GtkWrapBox
 *
 * Gets the vertical spacing.
 *
 * Returns: The vertical spacing.
 */
guint
gtk_wrap_box_get_vertical_spacing  (GtkWrapBox *box)
{
  g_return_val_if_fail (GTK_IS_WRAP_BOX (box), FALSE);

  return box->priv->vertical_spacing;
}

/**
 * gtk_wrap_box_set_horizontal_spacing:
 * @box: An #GtkWrapBox
 * @spacing: The spacing to use.
 *
 * Sets the horizontal space to add between children.
 */
void
gtk_wrap_box_set_horizontal_spacing (GtkWrapBox    *box,
                                     guint          spacing)
{
  GtkWrapBoxPrivate *priv;

  g_return_if_fail (GTK_IS_WRAP_BOX (box));

  priv = box->priv;

  if (priv->horizontal_spacing != spacing)
    {
      priv->horizontal_spacing = spacing;

      gtk_widget_queue_resize (GTK_WIDGET (box));

      g_object_notify (G_OBJECT (box), "horizontal-spacing");
    }
}

/**
 * gtk_wrap_box_get_horizontal_spacing:
 * @box: An #GtkWrapBox
 *
 * Gets the horizontal spacing.
 *
 * Returns: The horizontal spacing.
 */
guint
gtk_wrap_box_get_horizontal_spacing (GtkWrapBox *box)
{
  g_return_val_if_fail (GTK_IS_WRAP_BOX (box), FALSE);

  return box->priv->horizontal_spacing;
}

/**
 * gtk_wrap_box_set_minimum_line_children:
 * @box: An #GtkWrapBox
 * @n_children: The minimum amount of children per line.
 *
 * Sets the minimum amount of children to line up
 * in @box's orientation before wrapping.
 */
void
gtk_wrap_box_set_minimum_line_children (GtkWrapBox *box,
                                        guint       n_children)
{
  GtkWrapBoxPrivate *priv;

  g_return_if_fail (GTK_IS_WRAP_BOX (box));

  priv = box->priv;

  if (priv->minimum_line_children != n_children)
    {
      priv->minimum_line_children = n_children;

      gtk_widget_queue_resize (GTK_WIDGET (box));

      g_object_notify (G_OBJECT (box), "minimum-line-children");
    }
}

/**
 * gtk_wrap_box_get_minimum_line_children:
 * @box: An #GtkWrapBox
 *
 * Gets the minimum amount of children per line.
 *
 * Returns: The minimum amount of children per line.
 */
guint
gtk_wrap_box_get_minimum_line_children (GtkWrapBox *box)
{
  g_return_val_if_fail (GTK_IS_WRAP_BOX (box), FALSE);

  return box->priv->minimum_line_children;
}

/**
 * gtk_wrap_box_set_natural_line_children:
 * @box: An #GtkWrapBox
 * @n_children: The natural amount of children per line.
 *
 * Sets the natural length of items to request and
 * allocate space for in @box's orientation.
 *
 * Setting the natural amount of children per line
 * limits the overall natural size request to be no more
 * than @n_children items long in the given orientation.
 */
void
gtk_wrap_box_set_natural_line_children (GtkWrapBox *box,
                                        guint       n_children)
{
  GtkWrapBoxPrivate *priv;

  g_return_if_fail (GTK_IS_WRAP_BOX (box));

  priv = box->priv;

  if (priv->natural_line_children != n_children)
    {
      priv->natural_line_children = n_children;

      gtk_widget_queue_resize (GTK_WIDGET (box));

      g_object_notify (G_OBJECT (box), "natural-line-children");
    }
}

/**
 * gtk_wrap_box_get_natural_line_children:
 * @box: An #GtkWrapBox
 *
 * Gets the natural amount of children per line.
 *
 * Returns: The natural amount of children per line.
 */
guint
gtk_wrap_box_get_natural_line_children (GtkWrapBox *box)
{
  g_return_val_if_fail (GTK_IS_WRAP_BOX (box), FALSE);

  return box->priv->natural_line_children;
}


/**
 * gtk_wrap_box_insert_child:
 * @box: And #GtkWrapBox
 * @widget: the child #GtkWidget to add
 * @index: the position in the child list to insert, specify -1 to append to the list.
 * @xpad: horizontal spacing for this child
 * @ypad: vertical spacing for this child
 * @xexpand: whether this child expands horizontally
 * @yexpand: whether this child expands vertically
 * @xfill: whether this child fills its horizontal allocation
 * @yfill: whether this child fills its vertical allocation
 *
 * Adds a child to an #GtkWrapBox with its packing options set
 *
 */
void
gtk_wrap_box_insert_child (GtkWrapBox *box,
                           GtkWidget  *widget,
                           gint        index,
                           guint       xpad,
                           guint       ypad,
                           gboolean    xexpand,
                           gboolean    yexpand,
                           gboolean    xfill,
                           gboolean    yfill)
{
  GtkWrapBoxPrivate *priv;
  GtkWrapBoxChild   *child;
  GList             *list;

  g_return_if_fail (GTK_IS_WRAP_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = box->priv;

  list = g_list_find_custom (priv->children, widget,
                             (GCompareFunc)find_child_in_list);
  g_return_if_fail (list == NULL);

  child           = g_slice_new0 (GtkWrapBoxChild);
  child->widget   = widget;
  child->xpadding = xpad;
  child->ypadding = ypad;
  child->xexpand  = xexpand;
  child->yexpand  = yexpand;
  child->xfill    = xfill;
  child->yfill    = yfill;

  priv->children = g_list_insert (priv->children, child, index);

  gtk_widget_set_parent (widget, GTK_WIDGET (box));
}

/**
 * gtk_wrap_box_reorder_child:
 * @box: An #GtkWrapBox
 * @widget: The child to reorder
 * @index: The new child position
 *
 * Reorders the child @widget in @box's list of children.
 */
void
gtk_wrap_box_reorder_child (GtkWrapBox *box,
                            GtkWidget  *widget,
                            guint       index)
{
  GtkWrapBoxPrivate *priv;
  GtkWrapBoxChild   *child;
  GList             *list;

  g_return_if_fail (GTK_IS_WRAP_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = box->priv;

  list = g_list_find_custom (priv->children, widget,
                             (GCompareFunc)find_child_in_list);
  g_return_if_fail (list != NULL);

  if (g_list_position (priv->children, list) != index)
    {
      child = list->data;
      priv->children = g_list_delete_link (priv->children, list);
      priv->children = g_list_insert (priv->children, child, index);

      gtk_widget_queue_resize (GTK_WIDGET (box));
    }
}
