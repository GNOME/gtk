/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/**
 * SECTION:gtkbbox
 * @Short_description: Base class for GtkHButtonBox and GtkVButtonBox
 * @Title: GtkButtonBox
 * @See_also: #GtkVButtonBox, #GtkHButtonBox
 *
 * The primary purpose of this class is to keep track of the various properties
 * of #GtkHButtonBox and #GtkVButtonBox widgets.
 *
 * gtk_button_box_get_child_size() retrieves the minimum width and height
 * for widgets in a given button box.
 *
 * The internal padding of buttons can be retrieved and changed per button box
 * using gtk_button_box_get_child_ipadding() and
 * gtk_button_box_set_child_ipadding() respectively.
 *
 * gtk_button_box_get_spacing() and gtk_button_box_set_spacing() retrieve and
 * change default number of pixels between buttons, respectively.
 *
 * gtk_button_box_get_layout() and gtk_button_box_set_layout() retrieve and
 * alter the method used to spread the buttons in a button box across the
 * container, respectively.
 *
 * The main purpose of GtkButtonBox is to make sure the children have all the
 * same size. Therefore it ignores the homogeneous property which it inherited
 * from GtkBox, and always behaves as if homogeneous was %TRUE.
 */

#include "config.h"
#include "gtkbbox.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkintl.h"


struct _GtkButtonBoxPriv
{
  GtkButtonBoxStyle layout_style;
};

enum {
  PROP_0,
  PROP_LAYOUT_STYLE
};

enum {
  CHILD_PROP_0,
  CHILD_PROP_SECONDARY
};

#define GTK_BOX_SECONDARY_CHILD "gtk-box-secondary-child"

static void gtk_button_box_set_property       (GObject           *object,
                                               guint              prop_id,
                                               const GValue      *value,
                                               GParamSpec        *pspec);
static void gtk_button_box_get_property       (GObject           *object,
                                               guint              prop_id,
                                               GValue            *value,
                                               GParamSpec        *pspec);
static void gtk_button_box_size_request       (GtkWidget         *widget,
                                               GtkRequisition    *requisition);
static void gtk_button_box_size_allocate      (GtkWidget         *widget,
                                               GtkAllocation     *allocation);
static void gtk_button_box_remove             (GtkContainer      *container,
                                               GtkWidget         *widget);
static void gtk_button_box_set_child_property (GtkContainer      *container,
                                               GtkWidget         *child,
                                               guint              property_id,
                                               const GValue      *value,
                                               GParamSpec        *pspec);
static void gtk_button_box_get_child_property (GtkContainer      *container,
                                               GtkWidget         *child,
                                               guint              property_id,
                                               GValue            *value,
                                               GParamSpec        *pspec);

#define DEFAULT_CHILD_MIN_WIDTH 85
#define DEFAULT_CHILD_MIN_HEIGHT 27
#define DEFAULT_CHILD_IPAD_X 4
#define DEFAULT_CHILD_IPAD_Y 0
#define DEFAULT_LAYOUT_STYLE GTK_BUTTONBOX_EDGE

G_DEFINE_TYPE (GtkButtonBox, gtk_button_box, GTK_TYPE_BOX)

static void
gtk_button_box_class_init (GtkButtonBoxClass *class)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  gobject_class->set_property = gtk_button_box_set_property;
  gobject_class->get_property = gtk_button_box_get_property;

  widget_class->size_request = gtk_button_box_size_request;
  widget_class->size_allocate = gtk_button_box_size_allocate;

  container_class->remove = gtk_button_box_remove;
  container_class->set_child_property = gtk_button_box_set_child_property;
  container_class->get_child_property = gtk_button_box_get_child_property;

  /* FIXME we need to override the "spacing" property on GtkBox once
   * libgobject allows that.
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("child-min-width",
                                                             P_("Minimum child width"),
                                                             P_("Minimum width of buttons inside the box"),
                                                             0,
                                                             G_MAXINT,
                                                             DEFAULT_CHILD_MIN_WIDTH,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("child-min-height",
                                                             P_("Minimum child height"),
                                                             P_("Minimum height of buttons inside the box"),
                                                             0,
                                                             G_MAXINT,
                                                             DEFAULT_CHILD_MIN_HEIGHT,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("child-internal-pad-x",
                                                             P_("Child internal width padding"),
                                                             P_("Amount to increase child's size on either side"),
                                                             0,
                                                             G_MAXINT,
                                                             DEFAULT_CHILD_IPAD_X,
                                                             GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("child-internal-pad-y",
                                                             P_("Child internal height padding"),
                                                             P_("Amount to increase child's size on the top and bottom"),
                                                             0,
                                                             G_MAXINT,
                                                             DEFAULT_CHILD_IPAD_Y,
                                                             GTK_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_LAYOUT_STYLE,
                                   g_param_spec_enum ("layout-style",
                                                      P_("Layout style"),
                                                      P_("How to layout the buttons in the box. Possible values are spread, edge, start and end"),
                                                      GTK_TYPE_BUTTON_BOX_STYLE,
                                                      DEFAULT_LAYOUT_STYLE,
                                                      GTK_PARAM_READWRITE));

  gtk_container_class_install_child_property (container_class,
                                              CHILD_PROP_SECONDARY,
                                              g_param_spec_boolean ("secondary", 
                                                                    P_("Secondary"),
                                                                    P_("If TRUE, the child appears in a secondary group of children, suitable for, e.g., help buttons"),
                                                                    FALSE,
                                                                    GTK_PARAM_READWRITE));

  g_type_class_add_private (class, sizeof (GtkButtonBoxPriv));
}

static void
gtk_button_box_init (GtkButtonBox *button_box)
{
  GtkButtonBoxPriv *priv;

  button_box->priv = G_TYPE_INSTANCE_GET_PRIVATE (button_box,
                                                  GTK_TYPE_BUTTON_BOX,
                                                  GtkButtonBoxPriv);
  priv = button_box->priv;

  gtk_box_set_spacing (GTK_BOX (button_box), 0);
  priv->layout_style = DEFAULT_LAYOUT_STYLE;
}

static void
gtk_button_box_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_LAYOUT_STYLE:
      gtk_button_box_set_layout (GTK_BUTTON_BOX (object),
                                 g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_box_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkButtonBoxPriv *priv = GTK_BUTTON_BOX (object)->priv;

  switch (prop_id)
    {
    case PROP_LAYOUT_STYLE:
      g_value_set_enum (value, priv->layout_style);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_box_set_child_property (GtkContainer *container,
                                   GtkWidget    *child,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_SECONDARY:
      gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (container), child,
                                          g_value_get_boolean (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_button_box_get_child_property (GtkContainer *container,
                                   GtkWidget    *child,
                                   guint         property_id,
                                   GValue       *value,
                                   GParamSpec   *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_SECONDARY:
      g_value_set_boolean (value,
                           gtk_button_box_get_child_secondary (GTK_BUTTON_BOX (container),
                                                               child));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_button_box_remove (GtkContainer *container,
                       GtkWidget    *widget)
{
  /* clear is_secondary flag in case the widget
   * is added to another container
   */
  gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (container),
                                      widget,
                                      FALSE);

  GTK_CONTAINER_CLASS (gtk_button_box_parent_class)->remove (container, widget);
}

/**
 * gtk_button_box_set_layout:
 * @widget: a #GtkButtonBox
 * @layout_style: the new layout style
 *
 * Changes the way buttons are arranged in their container.
 */
void
gtk_button_box_set_layout (GtkButtonBox      *widget,
                           GtkButtonBoxStyle  layout_style)
{
  GtkButtonBoxPriv *priv;

  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));

  priv = widget->priv;

  if (priv->layout_style != layout_style)
    {
      priv->layout_style = layout_style;
      g_object_notify (G_OBJECT (widget), "layout-style");
      gtk_widget_queue_resize (GTK_WIDGET (widget));
    }
}

/**
 * gtk_button_box_get_layout:
 * @widget: a #GtkButtonBox
 *
 * Retrieves the method being used to arrange the buttons in a button box.
 *
 * Returns: the method used to layout buttons in @widget.
 */
GtkButtonBoxStyle
gtk_button_box_get_layout (GtkButtonBox *widget)
{
  g_return_val_if_fail (GTK_IS_BUTTON_BOX (widget), DEFAULT_LAYOUT_STYLE);

  return widget->priv->layout_style;
}

/**
 * gtk_button_box_get_child_secondary:
 * @widget: a #GtkButtonBox
 * @child: a child of @widget
 *
 * Returns whether @child should appear in a secondary group of children.
 *
 * Return value: whether @child should appear in a secondary group of children.
 *
 * Since: 2.4
 **/
gboolean
gtk_button_box_get_child_secondary (GtkButtonBox *widget,
                                    GtkWidget    *child)
{
  g_return_val_if_fail (GTK_IS_BUTTON_BOX (widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (child), FALSE);

  return (g_object_get_data (G_OBJECT (child), GTK_BOX_SECONDARY_CHILD) != NULL);
}

/**
 * gtk_button_box_set_child_secondary
 * @widget: a #GtkButtonBox
 * @child: a child of @widget
 * @is_secondary: if %TRUE, the @child appears in a secondary group of the
 *                button box.
 *
 * Sets whether @child should appear in a secondary group of children.
 * A typical use of a secondary child is the help button in a dialog.
 *
 * This group appears after the other children if the style
 * is %GTK_BUTTONBOX_START, %GTK_BUTTONBOX_SPREAD or
 * %GTK_BUTTONBOX_EDGE, and before the other children if the style
 * is %GTK_BUTTONBOX_END. For horizontal button boxes, the definition
 * of before/after depends on direction of the widget (see
 * gtk_widget_set_direction()). If the style is %GTK_BUTTONBOX_START
 * or %GTK_BUTTONBOX_END, then the secondary children are aligned at
 * the other end of the button box from the main children. For the
 * other styles, they appear immediately next to the main children.
 **/
void
gtk_button_box_set_child_secondary (GtkButtonBox *widget,
                                    GtkWidget    *child,
                                    gboolean      is_secondary)
{
  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (child->parent == GTK_WIDGET (widget));

  g_object_set_data (G_OBJECT (child),
                     GTK_BOX_SECONDARY_CHILD,
                     is_secondary ? GINT_TO_POINTER (1) : NULL);
  gtk_widget_child_notify (child, "secondary");

  if (gtk_widget_get_visible (GTK_WIDGET (widget)) &&
      gtk_widget_get_visible (child))
    gtk_widget_queue_resize (child);
}

/* Ask children how much space they require and round up
   to match minimum size and internal padding.
   Returns the size each single child should have. */
void
_gtk_button_box_child_requisition (GtkWidget *widget,
                                   int       *nvis_children,
                                   int       *nvis_secondaries,
                                   int       *width,
                                   int       *height)
{
  GtkButtonBoxPriv *priv;
  GtkButtonBox *bbox;
  GList *children, *list;
  gint nchildren;
  gint nsecondaries;
  gint needed_width;
  gint needed_height;
  GtkRequisition child_requisition;
  gint ipad_w;
  gint ipad_h;
  gint width_default;
  gint height_default;
  gint ipad_x_default;
  gint ipad_y_default;
  gint child_min_width;
  gint child_min_height;
  gint ipad_x;
  gint ipad_y;

  g_return_if_fail (GTK_IS_BUTTON_BOX (widget));

  bbox = GTK_BUTTON_BOX (widget);
  priv = bbox->priv;

  gtk_widget_style_get (widget,
                        "child-min-width", &width_default,
                        "child-min-height", &height_default,
                        "child-internal-pad-x", &ipad_x_default,
                        "child-internal-pad-y", &ipad_y_default,
                        NULL);

  child_min_width = width_default;
  child_min_height = height_default;
  ipad_x = ipad_x_default;
  ipad_y = ipad_y_default;

  nchildren = 0;
  nsecondaries = 0;
  list = children = _gtk_box_get_children (GTK_BOX (bbox));
  needed_width = child_min_width;
  needed_height = child_min_height;
  ipad_w = ipad_x * 2;
  ipad_h = ipad_y * 2;
  
  while (children)
    {
      GtkWidget *child;
      gboolean is_secondary;

      child = children->data;
      children = children->next;

      is_secondary = gtk_button_box_get_child_secondary (bbox, child);

      if (gtk_widget_get_visible (child))
        {
          nchildren += 1;
          gtk_widget_size_request (child, &child_requisition);

          if (child_requisition.width + ipad_w > needed_width)
            needed_width = child_requisition.width + ipad_w;
          if (child_requisition.height + ipad_h > needed_height)
            needed_height = child_requisition.height + ipad_h;
          if (is_secondary)
            nsecondaries++;
        }
    }

  g_list_free (list);

  if (nvis_children)
    *nvis_children = nchildren;
  if (nvis_secondaries)
    *nvis_secondaries = nsecondaries;
  if (width)
    *width = needed_width;
  if (height)
    *height = needed_height;
}

static void
gtk_button_box_size_request (GtkWidget      *widget,
                             GtkRequisition *requisition)
{
  GtkButtonBoxPriv *priv;
  GtkBox *box;
  GtkButtonBox *bbox;
  gint nvis_children;
  gint child_width;
  gint child_height;
  gint spacing;
  guint border_width;
  GtkOrientation orientation;

  box = GTK_BOX (widget);
  bbox = GTK_BUTTON_BOX (widget);
  priv = bbox->priv;

  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));
  spacing = gtk_box_get_spacing (box);

  _gtk_button_box_child_requisition (widget,
                                     &nvis_children,
                                     NULL,
                                     &child_width,
                                     &child_height);

  if (nvis_children == 0)
    {
      requisition->width = 0;
      requisition->height = 0;
    }
  else
    {
      switch (priv->layout_style)
        {
          case GTK_BUTTONBOX_SPREAD:
            if (orientation == GTK_ORIENTATION_HORIZONTAL)
              requisition->width =
                      nvis_children*child_width + ((nvis_children+1)*spacing);
            else
              requisition->height =
                      nvis_children*child_height + ((nvis_children+1)*spacing);

            break;
          case GTK_BUTTONBOX_EDGE:
          case GTK_BUTTONBOX_START:
          case GTK_BUTTONBOX_END:
          case GTK_BUTTONBOX_CENTER:
            if (orientation == GTK_ORIENTATION_HORIZONTAL)
              requisition->width =
                      nvis_children*child_width + ((nvis_children-1)*spacing);
            else
              requisition->height =
                      nvis_children*child_height + ((nvis_children-1)*spacing);

            break;
          default:
            g_assert_not_reached ();
            break;
        }

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        requisition->height = child_height;
      else
        requisition->width = child_width;
    }

  border_width = gtk_container_get_border_width (GTK_CONTAINER (box));
  requisition->width += border_width * 2;
  requisition->height += border_width * 2;
}

static void
gtk_button_box_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkButtonBoxPriv *priv;
  GtkBox *base_box;
  GtkButtonBox *box;
  GList *children, *list;
  GtkAllocation child_allocation;
  gint nvis_children;
  gint n_secondaries;
  gint child_width;
  gint child_height;
  gint x = 0;
  gint y = 0;
  gint secondary_x = 0;
  gint secondary_y = 0;
  gint width = 0;
  gint height = 0;
  gint childspace;
  gint childspacing = 0;
  gint spacing;
  guint border_width;
  GtkOrientation orientation;

  base_box = GTK_BOX (widget);
  box = GTK_BUTTON_BOX (widget);
  priv = box->priv;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (box));
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (widget));
  spacing = gtk_box_get_spacing (base_box);
  _gtk_button_box_child_requisition (widget,
                                     &nvis_children,
                                     &n_secondaries,
                                     &child_width,
                                     &child_height);
  widget->allocation = *allocation;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    width = allocation->width - border_width*2;
  else
    height = allocation->height - border_width*2;

  switch (priv->layout_style)
    {
      case GTK_BUTTONBOX_SPREAD:

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            childspacing = (width - (nvis_children * child_width))
                    / (nvis_children + 1);
            x = allocation->x + border_width + childspacing;
            secondary_x = x + ((nvis_children - n_secondaries)
                            * (child_width + childspacing));
          }
        else
          {
            childspacing = (height - (nvis_children * child_height))
                    / (nvis_children + 1);
            y = allocation->y + border_width + childspacing;
            secondary_y = y + ((nvis_children - n_secondaries)
                            * (child_height + childspacing));
          }

        break;

      case GTK_BUTTONBOX_EDGE:

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            if (nvis_children >= 2)
              {
                childspacing = (width - (nvis_children * child_width))
                      / (nvis_children - 1);
                x = allocation->x + border_width;
                secondary_x = x + ((nvis_children - n_secondaries)
                                   * (child_width + childspacing));
              }
            else
              {
                /* one or zero children, just center */
                childspacing = width;
                x = secondary_x = allocation->x
                      + (allocation->width - child_width) / 2;
              }
          }
        else
          {
            if (nvis_children >= 2)
              {
                childspacing = (height - (nvis_children*child_height))
                        / (nvis_children-1);
                y = allocation->y + border_width;
                secondary_y = y + ((nvis_children - n_secondaries)
                                * (child_height + childspacing));
              }
            else
              {
                /* one or zero children, just center */
                childspacing = height;
                y = secondary_y = allocation->y
                        + (allocation->height - child_height) / 2;
              }
          }

        break;

      case GTK_BUTTONBOX_START:

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            childspacing = spacing;
            x = allocation->x + border_width;
            secondary_x = allocation->x + allocation->width
              - child_width * n_secondaries
              - spacing * (n_secondaries - 1)
              - border_width;
          }
        else
          {
            childspacing = spacing;
            y = allocation->y + border_width;
            secondary_y = allocation->y + allocation->height
              - child_height * n_secondaries
              - spacing * (n_secondaries - 1)
              - border_width;
          }

        break;

      case GTK_BUTTONBOX_END:

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            childspacing = spacing;
            x = allocation->x + allocation->width
              - child_width * (nvis_children - n_secondaries)
              - spacing * (nvis_children - n_secondaries - 1)
              - border_width;
            secondary_x = allocation->x + border_width;
          }
        else
          {
            childspacing = spacing;
            y = allocation->y + allocation->height
              - child_height * (nvis_children - n_secondaries)
              - spacing * (nvis_children - n_secondaries - 1)
              - border_width;
            secondary_y = allocation->y + border_width;
          }

        break;

      case GTK_BUTTONBOX_CENTER:

        if (orientation == GTK_ORIENTATION_HORIZONTAL)
          {
            childspacing = spacing;
            x = allocation->x +
              (allocation->width
               - (child_width * (nvis_children - n_secondaries)
               + spacing * (nvis_children - n_secondaries - 1))) / 2
              + (n_secondaries * child_width + n_secondaries * spacing) / 2;
            secondary_x = allocation->x + border_width;
          }
        else
          {
            childspacing = spacing;
            y = allocation->y +
              (allocation->height
               - (child_height * (nvis_children - n_secondaries)
                  + spacing * (nvis_children - n_secondaries - 1))) / 2
              + (n_secondaries * child_height + n_secondaries * spacing) / 2;
            secondary_y = allocation->y + border_width;
          }

        break;

      default:
        g_assert_not_reached ();
        break;
    }

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
      {
        y = allocation->y + (allocation->height - child_height) / 2;
        childspace = child_width + childspacing;
      }
    else
      {
        x = allocation->x + (allocation->width - child_width) / 2;
        childspace = child_height + childspacing;
      }

  list = children = _gtk_box_get_children (GTK_BOX (box));

  while (children)
    {
      GtkWidget *child;
      gboolean is_secondary;

      child = children->data;
      children = children->next;

      is_secondary = gtk_button_box_get_child_secondary (box, child);

      if (gtk_widget_get_visible (child))
        {
          child_allocation.width = child_width;
          child_allocation.height = child_height;

          if (orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              child_allocation.y = y;

              if (is_secondary)
                {
                  child_allocation.x = secondary_x;
                  secondary_x += childspace;
                }
              else
                {
                  child_allocation.x = x;
                  x += childspace;
                }

              if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
                  child_allocation.x = (allocation->x + allocation->width)
                          - (child_allocation.x + child_width - allocation->x);
            }
          else
            {
              child_allocation.x = x;

              if (is_secondary)
                {
                  child_allocation.y = secondary_y;
                  secondary_y += childspace;
                }
              else
                {
                  child_allocation.y = y;
                  y += childspace;
                }
            }

          gtk_widget_size_allocate (child, &child_allocation);
        }
    }

  g_list_free (list);
}

/**
 * gtk_button_box_new:
 * @orientation: the box' orientation.
 *
 * Creates a new #GtkButtonBox.
 *
 * Return value: a new #GtkButtonBox.
 *
 * Since: 3.0
 */
GtkWidget *
gtk_button_box_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_BUTTON_BOX,
                       "orientation", orientation,
                       NULL);
}
