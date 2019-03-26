/* gtkfixedlayout.c: Fixed positioning layout manager
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Copyright 2019 GNOME Foundation
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

/**
 * SECTION:gtkfixedlayout
 * @Short_description: A layout manager that allows positioning at fixed
 *   coordinates
 * @Title: GtkFixedLayout
 *
 * #GtkFixedLayout is a layout manager which can place child widgets
 * at fixed positions, and with fixed sizes.
 *
 * Most applications should never use this layout manager; fixed positioning
 * and sizing requires constant recalculations on where children need to be
 * positioned and sized. Other layout managers perform this kind of work
 * internally so that application developers don't need to do it. Specifically,
 * widgets positioned in a fixed layout manager will need to take into account:
 *
 * - Themes, which may change widget sizes.
 *
 * - Fonts other than the one you used to write the app will of course
 *   change the size of widgets containing text; keep in mind that
 *   users may use a larger font because of difficulty reading the
 *   default, or they may be using a different OS that provides different
 *   fonts.
 *
 * - Translation of text into other languages changes its size. Also,
 *   display of non-English text will use a different font in many
 *   cases.
 *
 * In addition, #GtkFixedLayout does not pay attention to text direction and
 * thus may produce unwanted results if your app is run under right-to-left
 * languages such as Hebrew or Arabic. That is: normally GTK will order
 * containers appropriately depending on the text direction, e.g. to put labels
 * to the right of the thing they label when using an RTL language;
 * #GtkFixedLayout won't be able to do that for you.
 *
 * Finally, fixed positioning makes it kind of annoying to add/remove GUI
 * elements, since you have to reposition all the other  elements. This is a
 * long-term maintenance problem for your application.
 */

#include "config.h"

#include "gtkfixedlayout.h"

#include "gtkintl.h"
#include "gtklayoutchild.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"

#include <graphene-gobject.h>

struct _GtkFixedLayout
{
  GtkLayoutManager parent_instance;
};

struct _GtkFixedLayoutChild
{
  GtkLayoutChild parent_instance;

  graphene_point_t position;
};

enum
{
  PROP_CHILD_POSITION = 1,

  N_CHILD_PROPERTIES
};

static GParamSpec *child_props[N_CHILD_PROPERTIES];

G_DEFINE_TYPE (GtkFixedLayoutChild, gtk_fixed_layout_child, GTK_TYPE_LAYOUT_CHILD)

static void
gtk_fixed_layout_child_set_property (GObject      *gobject,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkFixedLayoutChild *self = GTK_FIXED_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_POSITION:
      gtk_fixed_layout_child_set_position (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_fixed_layout_child_get_property (GObject    *gobject,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkFixedLayoutChild *self = GTK_FIXED_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_CHILD_POSITION:
      g_value_set_boxed (value, &self->position);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_fixed_layout_child_class_init (GtkFixedLayoutChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_fixed_layout_child_set_property;
  gobject_class->get_property = gtk_fixed_layout_child_get_property;

  child_props[PROP_CHILD_POSITION] =
    g_param_spec_boxed ("position",
                        P_("Position"),
                        P_("The position of a child of a fixed layout"),
                        GRAPHENE_TYPE_POINT,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_CHILD_PROPERTIES, child_props);
}

static void
gtk_fixed_layout_child_init (GtkFixedLayoutChild *self)
{
}

/**
 * gtk_fixed_layout_child_set_position:
 * @child: a #GtkFixedLayoutChild
 * @position: the position of the child
 *
 * Sets the position of the child of a #GtkFixedLayout.
 */
void
gtk_fixed_layout_child_set_position (GtkFixedLayoutChild    *child,
                                     const graphene_point_t *position)
{
  GtkLayoutManager *layout;

  g_return_if_fail (GTK_IS_FIXED_LAYOUT_CHILD (child));
  g_return_if_fail (position != NULL);

  if (graphene_point_equal (&child->position, position))
    return;

  child->position = *position;

  layout = gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child));
  gtk_layout_manager_layout_changed (layout);

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CHILD_POSITION]);
}

/**
 * gtk_fixed_layout_child_get_position:
 * @child: a #GtkFixedLayoutChild
 * @position: (out caller-allocates): the position of the child
 *
 * Retrieves the position of the child of a #GtkFixedLayout.
 */
void
gtk_fixed_layout_child_get_position (GtkFixedLayoutChild *child,
                                     graphene_point_t    *position)
{
  g_return_if_fail (GTK_IS_FIXED_LAYOUT_CHILD (child));
  g_return_if_fail (position != NULL);

  *position = child->position;
}

G_DEFINE_TYPE (GtkFixedLayout, gtk_fixed_layout, GTK_TYPE_LAYOUT_MANAGER)

static void
gtk_fixed_layout_measure (GtkLayoutManager *layout_manager,
                          GtkWidget        *widget,
                          GtkOrientation    orientation,
                          int               for_size,
                          int              *minimum,
                          int              *natural,
                          int              *minimum_baseline,
                          int              *natural_baseline)
{
  GtkFixedLayoutChild *child_info;
  GtkWidget *child;
  int minimum_size = 0;
  int natural_size = 0;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      int child_min = 0;
      int child_nat = 0;

      if (!gtk_widget_get_visible (child))
        continue;

      child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (layout_manager, child));

      gtk_widget_measure (child, orientation, -1, &child_min, &child_nat, NULL, NULL);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          minimum_size = MAX (minimum_size, child_info->position.x + child_min);
          natural_size = MAX (natural_size, child_info->position.x + child_nat);
        }
      else
        {
          minimum_size = MAX (minimum_size, child_info->position.y + child_min);
          natural_size = MAX (natural_size, child_info->position.y + child_nat);
        }
    }

  if (minimum != NULL)
    *minimum = minimum_size;
  if (natural != NULL)
    *natural = natural_size;
}

static void
gtk_fixed_layout_allocate (GtkLayoutManager *layout_manager,
                           GtkWidget        *widget,
                           int               width,
                           int               height,
                           int               baseline)
{
  GtkFixedLayoutChild *child_info;
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkRequisition child_req;

      if (!gtk_widget_get_visible (child))
        continue;

      child_info = GTK_FIXED_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (layout_manager, child));
      gtk_widget_get_preferred_size (child, &child_req, NULL);

      gtk_widget_size_allocate (child,
                                &(GtkAllocation) {
                                  .x = child_info->position.x,
                                  .y = child_info->position.y,
                                  .width = child_req.width,
                                  .height = child_req.height,
                                }, -1);
    }
}

static GtkLayoutChild *
gtk_fixed_layout_create_layout_child (GtkLayoutManager *manager,
                                      GtkWidget        *widget,
                                      GtkWidget        *for_child)
{
  return g_object_new (GTK_TYPE_FIXED_LAYOUT_CHILD,
                       "layout-manager", manager,
                       "child-widget", for_child,
                       NULL);
}

static void
gtk_fixed_layout_class_init (GtkFixedLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->measure = gtk_fixed_layout_measure;
  layout_class->allocate = gtk_fixed_layout_allocate;
  layout_class->create_layout_child = gtk_fixed_layout_create_layout_child;
}

static void
gtk_fixed_layout_init (GtkFixedLayout *self)
{
}

GtkLayoutManager *
gtk_fixed_layout_new (void)
{
  return g_object_new (GTK_TYPE_FIXED_LAYOUT, NULL);
}
