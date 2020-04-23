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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/**
 * SECTION:gtkbox
 * @Short_description: A container for packing widgets in a single row or column
 * @Title: GtkBox
 * @See_also: #GtkGrid
 *
 * The GtkBox widget arranges child widgets into a single row or column,
 * depending upon the value of its #GtkOrientable:orientation property. Within
 * the other dimension, all children are allocated the same size. Of course,
 * the #GtkWidget:halign and #GtkWidget:valign properties can be used on
 * the children to influence their allocation.
 *
 * Use repeated calls to gtk_box_append() to pack widgets into a GtkBox
 * from start to end. Use gtk_box_remove() to remove widgets from the GtkBox.
 * gtk_box_insert_child_after() can be used to add a child at a particular position.
 *
 * Use gtk_box_set_homogeneous() to specify whether or not all children
 * of the GtkBox are forced to get the same amount of space.
 *
 * Use gtk_box_set_spacing() to determine how much space will be
 * minimally placed between all children in the GtkBox. Note that
 * spacing is added between the children.
 *
 * Use gtk_box_reorder_child_after() to move a child to a different
 * place in the box.
 *
 * # CSS nodes
 *
 * GtkBox uses a single CSS node with name box.
 */

#include "config.h"

#include "gtkbox.h"
#include "gtkboxlayout.h"
#include "gtkbuildable.h"
#include "gtkcsspositionvalueprivate.h"
#include "gtkintl.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtktypebuiltins.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"


enum {
  PROP_0,
  PROP_SPACING,
  PROP_HOMOGENEOUS,
  PROP_BASELINE_POSITION,

  /* orientable */
  PROP_ORIENTATION,
  LAST_PROP = PROP_ORIENTATION
};

typedef struct
{
  GtkOrientation  orientation;
  gint16          spacing;

  guint           homogeneous    : 1;
  guint           baseline_pos   : 2;
} GtkBoxPrivate;

static GParamSpec *props[LAST_PROP] = { NULL, };

static void gtk_box_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkBox, gtk_box, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkBox)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_box_buildable_iface_init))


static void
gtk_box_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  GtkBox *box = GTK_BOX (object);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkLayoutManager *box_layout = gtk_widget_get_layout_manager (GTK_WIDGET (box));

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (priv->orientation != orientation)
          {
            priv->orientation = orientation;
            gtk_orientable_set_orientation (GTK_ORIENTABLE (box_layout),
                                            priv->orientation);
            gtk_widget_update_orientation (GTK_WIDGET (box), priv->orientation);
            g_object_notify (object, "orientation");
          }
      }
      break;
    case PROP_SPACING:
      gtk_box_set_spacing (box, g_value_get_int (value));
      break;
    case PROP_BASELINE_POSITION:
      gtk_box_set_baseline_position (box, g_value_get_enum (value));
      break;
    case PROP_HOMOGENEOUS:
      gtk_box_set_homogeneous (box, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_box_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  GtkBox *box = GTK_BOX (object);
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);
  GtkBoxLayout *box_layout = GTK_BOX_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (box)));

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_SPACING:
      g_value_set_int (value, gtk_box_layout_get_spacing (box_layout));
      break;
    case PROP_BASELINE_POSITION:
      g_value_set_enum (value, gtk_box_layout_get_baseline_position (box_layout));
      break;
    case PROP_HOMOGENEOUS:
      g_value_set_boolean (value, gtk_box_layout_get_homogeneous (box_layout));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_box_compute_expand (GtkWidget *widget,
                        gboolean  *hexpand_p,
                        gboolean  *vexpand_p)
{
  GtkWidget *w;
  gboolean hexpand = FALSE;
  gboolean vexpand = FALSE;

  for (w = gtk_widget_get_first_child (widget);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      hexpand = hexpand || gtk_widget_compute_expand (w, GTK_ORIENTATION_HORIZONTAL);
      vexpand = vexpand || gtk_widget_compute_expand (w, GTK_ORIENTATION_VERTICAL);
    }

  *hexpand_p = hexpand;
  *vexpand_p = vexpand;
}

static GtkSizeRequestMode
gtk_box_get_request_mode (GtkWidget *widget)
{
  GtkWidget *w;
  int wfh = 0, hfw = 0;

  for (w = gtk_widget_get_first_child (widget);
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      GtkSizeRequestMode mode = gtk_widget_get_request_mode (w);

      switch (mode)
        {
        case GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH:
          hfw ++;
          break;
        case GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT:
          wfh ++;
          break;
        case GTK_SIZE_REQUEST_CONSTANT_SIZE:
        default:
          break;
        }
    }

  if (hfw == 0 && wfh == 0)
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  else
    return wfh > hfw ?
        GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT :
        GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static void
gtk_box_dispose (GObject *object)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (gtk_box_parent_class)->dispose (object);
}

static void
gtk_box_class_init (GtkBoxClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = gtk_box_set_property;
  object_class->get_property = gtk_box_get_property;
  object_class->dispose = gtk_box_dispose;

  widget_class->focus = gtk_widget_focus_child;
  widget_class->compute_expand = gtk_box_compute_expand;
  widget_class->get_request_mode = gtk_box_get_request_mode;

  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  props[PROP_SPACING] =
    g_param_spec_int ("spacing",
                      P_("Spacing"),
                      P_("The amount of space between children"),
                      0, G_MAXINT, 0,
                      GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_HOMOGENEOUS] =
    g_param_spec_boolean ("homogeneous",
                          P_("Homogeneous"),
                          P_("Whether the children should all be the same size"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  props[PROP_BASELINE_POSITION] =
    g_param_spec_enum ("baseline-position",
                       P_("Baseline position"),
                       P_("The position of the baseline aligned widgets if extra space is available"),
                       GTK_TYPE_BASELINE_POSITION,
                       GTK_BASELINE_POSITION_CENTER,
                       GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_FILLER);
  gtk_widget_class_set_css_name (widget_class, I_("box"));
}
static void
gtk_box_init (GtkBox *box)
{
  GtkBoxPrivate *priv = gtk_box_get_instance_private (box);

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  gtk_widget_update_orientation (GTK_WIDGET (box), priv->orientation);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_box_buildable_add_child (GtkBuildable *buildable,
                             GtkBuilder   *builder,
                             GObject      *child,
                             const gchar  *type)
{
  if (GTK_IS_WIDGET (child))
    gtk_box_append (GTK_BOX (buildable), GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_box_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_box_buildable_add_child;
}

/**
 * gtk_box_new:
 * @orientation: the box’s orientation.
 * @spacing: the number of pixels to place by default between children.
 *
 * Creates a new #GtkBox.
 *
 * Returns: a new #GtkBox.
 **/
GtkWidget*
gtk_box_new (GtkOrientation orientation,
             gint           spacing)
{
  return g_object_new (GTK_TYPE_BOX,
                       "orientation", orientation,
                       "spacing", spacing,
                       NULL);
}

/**
 * gtk_box_set_homogeneous:
 * @box: a #GtkBox
 * @homogeneous: a boolean value, %TRUE to create equal allotments,
 *   %FALSE for variable allotments
 *
 * Sets the #GtkBox:homogeneous property of @box, controlling
 * whether or not all children of @box are given equal space
 * in the box.
 */
void
gtk_box_set_homogeneous (GtkBox  *box,
			 gboolean homogeneous)
{
  GtkBoxLayout *box_layout;

  g_return_if_fail (GTK_IS_BOX (box));

  homogeneous = !!homogeneous;

  box_layout = GTK_BOX_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (box)));
  if (homogeneous == gtk_box_layout_get_homogeneous (box_layout))
    return;

  gtk_box_layout_set_homogeneous (box_layout, homogeneous);
  g_object_notify_by_pspec (G_OBJECT (box), props[PROP_HOMOGENEOUS]);
}

/**
 * gtk_box_get_homogeneous:
 * @box: a #GtkBox
 *
 * Returns whether the box is homogeneous (all children are the
 * same size). See gtk_box_set_homogeneous().
 *
 * Returns: %TRUE if the box is homogeneous.
 **/
gboolean
gtk_box_get_homogeneous (GtkBox *box)
{
  GtkLayoutManager *box_layout;

  g_return_val_if_fail (GTK_IS_BOX (box), FALSE);

  box_layout = gtk_widget_get_layout_manager (GTK_WIDGET (box));

  return gtk_box_layout_get_homogeneous (GTK_BOX_LAYOUT (box_layout));
}

/**
 * gtk_box_set_spacing:
 * @box: a #GtkBox
 * @spacing: the number of pixels to put between children
 *
 * Sets the #GtkBox:spacing property of @box, which is the
 * number of pixels to place between children of @box.
 */
void
gtk_box_set_spacing (GtkBox *box,
		     gint    spacing)
{
  GtkBoxLayout *box_layout;

  g_return_if_fail (GTK_IS_BOX (box));

  box_layout = GTK_BOX_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (box)));
  if (spacing == gtk_box_layout_get_spacing (box_layout))
    return;

  gtk_box_layout_set_spacing (box_layout, spacing);
  g_object_notify_by_pspec (G_OBJECT (box), props[PROP_SPACING]);
}

/**
 * gtk_box_get_spacing:
 * @box: a #GtkBox
 *
 * Gets the value set by gtk_box_set_spacing().
 *
 * Returns: spacing between children
 **/
gint
gtk_box_get_spacing (GtkBox *box)
{
  GtkLayoutManager *box_layout;

  g_return_val_if_fail (GTK_IS_BOX (box), 0);

  box_layout = gtk_widget_get_layout_manager (GTK_WIDGET (box));

  return gtk_box_layout_get_spacing (GTK_BOX_LAYOUT (box_layout));
}

/**
 * gtk_box_set_baseline_position:
 * @box: a #GtkBox
 * @position: a #GtkBaselinePosition
 *
 * Sets the baseline position of a box. This affects
 * only horizontal boxes with at least one baseline aligned
 * child. If there is more vertical space available than requested,
 * and the baseline is not allocated by the parent then
 * @position is used to allocate the baseline wrt the
 * extra space available.
 */
void
gtk_box_set_baseline_position (GtkBox             *box,
			       GtkBaselinePosition position)
{
  GtkBoxLayout *box_layout;

  g_return_if_fail (GTK_IS_BOX (box));

  box_layout = GTK_BOX_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (box)));
  if (position == gtk_box_layout_get_baseline_position (box_layout))
    return;

  gtk_box_layout_set_baseline_position (box_layout, position);
  g_object_notify_by_pspec (G_OBJECT (box), props[PROP_BASELINE_POSITION]);
}

/**
 * gtk_box_get_baseline_position:
 * @box: a #GtkBox
 *
 * Gets the value set by gtk_box_set_baseline_position().
 *
 * Returns: the baseline position
 **/
GtkBaselinePosition
gtk_box_get_baseline_position (GtkBox *box)
{
  GtkLayoutManager *box_layout;

  g_return_val_if_fail (GTK_IS_BOX (box), GTK_BASELINE_POSITION_CENTER);

  box_layout = gtk_widget_get_layout_manager (GTK_WIDGET (box));

  return gtk_box_layout_get_baseline_position (GTK_BOX_LAYOUT (box_layout));
}

/**
 * gtk_box_insert_child_after:
 * @box: a #GtkBox
 * @child: the #GtkWidget to insert
 * @sibling: (nullable): the sibling to move @child after, or %NULL
 *
 * Inserts @child in the position after @sibling in the list
 * of @box children. If @sibling is %NULL, insert @child at
 * the first position.
 */
void
gtk_box_insert_child_after (GtkBox    *box,
                            GtkWidget *child,
                            GtkWidget *sibling)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  widget = GTK_WIDGET (box);

  if (sibling)
    {
      g_return_if_fail (GTK_IS_WIDGET (sibling));
      g_return_if_fail (gtk_widget_get_parent (sibling) == widget);
    }

  if (child == sibling)
    return;

  gtk_widget_insert_after (child, widget, sibling);
  gtk_css_node_insert_after (gtk_widget_get_css_node (widget),
                             gtk_widget_get_css_node (child),
                             sibling ? gtk_widget_get_css_node (sibling) : NULL);
}

/**
 * gtk_box_reorder_child_after:
 * @box: a #GtkBox
 * @child: the #GtkWidget to move, must be a child of @box
 * @sibling: (nullable): the sibling to move @child after, or %NULL
 *
 * Moves @child to the position after @sibling in the list
 * of @box children. If @sibling is %NULL, move @child to
 * the first position.
 */
void
gtk_box_reorder_child_after (GtkBox    *box,
                             GtkWidget *child,
                             GtkWidget *sibling)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == (GtkWidget *)box);

 widget = GTK_WIDGET (box);

  if (sibling)
    {
      g_return_if_fail (GTK_IS_WIDGET (sibling));
      g_return_if_fail (gtk_widget_get_parent (sibling) == widget);
    }

  if (child == sibling)
    return;

  gtk_widget_insert_after (child, widget, sibling);
  gtk_css_node_insert_after (gtk_widget_get_css_node (widget),
                             gtk_widget_get_css_node (child),
                             sibling ? gtk_widget_get_css_node (sibling) : NULL);
}

/**
 * gtk_box_append:
 * @box: a #GtkBox
 * @child: the #GtkWidget to append
 *
 * Adds @child as the last child to @box.
 */
void
gtk_box_append (GtkBox    *box,
                GtkWidget *child)
{
  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  gtk_widget_insert_before (child, GTK_WIDGET (box), NULL);
}

/**
 * gtk_box_prepend:
 * @box: a #GtkBox
 * @child: the #GtkWidget to prepend
 *
 * Adds @child as the first child to @box.
 */
void
gtk_box_prepend (GtkBox    *box,
                 GtkWidget *child)
{
  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == NULL);

  gtk_widget_insert_after (child, GTK_WIDGET (box), NULL);
}

/**
 * gtk_box_remove:
 * @box: a #GtkBox
 * @child: the child to remove
 *
 * Removes a child widget from @box, after it has been
 * added with gtk_box_append(), gtk_box_prepend(), or
 * gtk_box_insert_child_after().
 */
void
gtk_box_remove (GtkBox    *box,
                GtkWidget *child)
{
  g_return_if_fail (GTK_IS_BOX (box));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == (GtkWidget *)box);

  gtk_widget_unparent (child);
}
