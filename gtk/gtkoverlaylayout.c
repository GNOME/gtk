/* gtkoverlaylayout.c: Overlay layout manager
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Copyright 2019 Red Hat, Inc.
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

#include "gtkoverlaylayoutprivate.h"

#include "gtkintl.h"
#include "gtklayoutchild.h"
#include "gtkoverlay.h"
#include "gtkprivate.h"
#include "gtkstylecontext.h"
#include "gtkwidgetprivate.h"

#include <graphene-gobject.h>

struct _GtkOverlayLayout
{
  GtkLayoutManager parent_instance;
};

struct _GtkOverlayLayoutChild
{
  GtkLayoutChild parent_instance;

  guint measure : 1;
  guint clip_overlay : 1;
};

enum
{
  PROP_MEASURE = 1,
  PROP_CLIP_OVERLAY,

  N_CHILD_PROPERTIES
};

static GParamSpec *child_props[N_CHILD_PROPERTIES];

G_DEFINE_TYPE (GtkOverlayLayoutChild, gtk_overlay_layout_child, GTK_TYPE_LAYOUT_CHILD)

static void
gtk_overlay_layout_child_set_property (GObject      *gobject,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkOverlayLayoutChild *self = GTK_OVERLAY_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_MEASURE:
      gtk_overlay_layout_child_set_measure (self, g_value_get_boolean (value));
      break;

    case PROP_CLIP_OVERLAY:
      gtk_overlay_layout_child_set_clip_overlay (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_overlay_layout_child_get_property (GObject    *gobject,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkOverlayLayoutChild *self = GTK_OVERLAY_LAYOUT_CHILD (gobject);

  switch (prop_id)
    {
    case PROP_MEASURE:
      g_value_set_boolean (value, self->measure);
      break;

    case PROP_CLIP_OVERLAY:
      g_value_set_boolean (value, self->clip_overlay);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_overlay_layout_child_finalize (GObject *gobject)
{
  //GtkOverlayLayoutChild *self = GTK_OVERLAY_LAYOUT_CHILD (gobject);

  G_OBJECT_CLASS (gtk_overlay_layout_child_parent_class)->finalize (gobject);
}

static void
gtk_overlay_layout_child_class_init (GtkOverlayLayoutChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_overlay_layout_child_set_property;
  gobject_class->get_property = gtk_overlay_layout_child_get_property;
  gobject_class->finalize = gtk_overlay_layout_child_finalize;

  child_props[PROP_MEASURE] =
    g_param_spec_boolean ("measure",
                          P_("Measure"),
                          P_("Include in size measurement"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);
  child_props[PROP_CLIP_OVERLAY] =
    g_param_spec_boolean ("clip-overlay",
                          P_("Clip Overlay"),
                          P_("Clip the overlay child widget so as to fit the parent"),
                          FALSE,
                          GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_CHILD_PROPERTIES, child_props);
}

static void
gtk_overlay_layout_child_init (GtkOverlayLayoutChild *self)
{
}

/**
 * gtk_overlay_layout_child_set_measure:
 * @child: a #GtkOverlayLayoutChild
 * @measure: whether to measure this child
 *
 * Sets whether to measure this child.
 */
void
gtk_overlay_layout_child_set_measure (GtkOverlayLayoutChild *child,
                                      gboolean               measure)
{
  GtkLayoutManager *layout;

  g_return_if_fail (GTK_IS_OVERLAY_LAYOUT_CHILD (child));

  if (child->measure == measure)
    return;

  child->measure = measure;

  layout = gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child));
  gtk_layout_manager_layout_changed (layout);

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_MEASURE]);
}

/**
 * gtk_overlay_layout_child_get_measure:
 * @child: a #GtkOverlayLayoutChild
 *
 * Retrieves whether the child is measured.
 *
 * Returns: (transfer none) (nullable): whether the child is measured
 */
gboolean
gtk_overlay_layout_child_get_measure (GtkOverlayLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_OVERLAY_LAYOUT_CHILD (child), FALSE);

  return child->measure;
}

/**
 * gtk_overlay_layout_child_set_clip_overlay:
 * @child: a #GtkOverlayLayoutChild
 * @clip_overlay: whether to clip this child
 *
 * Sets whether to clip this child.
 */
void
gtk_overlay_layout_child_set_clip_overlay (GtkOverlayLayoutChild *child,
                                           gboolean               clip_overlay)
{
  GtkLayoutManager *layout;

  g_return_if_fail (GTK_IS_OVERLAY_LAYOUT_CHILD (child));

  if (child->clip_overlay == clip_overlay)
    return;

  child->clip_overlay = clip_overlay;

  layout = gtk_layout_child_get_layout_manager (GTK_LAYOUT_CHILD (child));
  gtk_layout_manager_layout_changed (layout);

  g_object_notify_by_pspec (G_OBJECT (child), child_props[PROP_CLIP_OVERLAY]);
}

/**
 * gtk_overlay_layout_child_get_clip_overlay:
 * @child: a #GtkOverlayLayoutChild
 *
 * Retrieves whether the child is clipped.
 *
 * Returns: (transfer none) (nullable): whether the child is clipped
 */
gboolean
gtk_overlay_layout_child_get_clip_overlay (GtkOverlayLayoutChild *child)
{
  g_return_val_if_fail (GTK_IS_OVERLAY_LAYOUT_CHILD (child), FALSE);

  return child->clip_overlay;
}

G_DEFINE_TYPE (GtkOverlayLayout, gtk_overlay_layout, GTK_TYPE_LAYOUT_MANAGER)

static void
gtk_overlay_layout_measure (GtkLayoutManager *layout_manager,
                            GtkWidget        *widget,
                            GtkOrientation    orientation,
                            int               for_size,
                            int              *minimum,
                            int              *natural,
                            int              *minimum_baseline,
                            int              *natural_baseline)
{
  GtkOverlayLayoutChild *child_info;
  GtkWidget *child;
  int min, nat;
  GtkWidget *main_widget;

  main_widget = gtk_bin_get_child (GTK_BIN (widget));

  min = 0;
  nat = 0;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_should_layout (child))
        continue;

      child_info = GTK_OVERLAY_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (layout_manager, child));

      if (child == main_widget || child_info->measure)
        {
          int child_min, child_nat, child_min_baseline, child_nat_baseline;

          gtk_widget_measure (child,
                              orientation,
                              for_size,
                              &child_min, &child_nat,
                              &child_min_baseline, &child_nat_baseline);

          min = MAX (min, child_min);
          nat = MAX (nat, child_nat);
        }
    }

  if (minimum != NULL)
    *minimum = min;
  if (natural != NULL)
    *natural = nat;
}

static void
gtk_overlay_compute_child_allocation (GtkOverlay            *overlay,
                                      GtkWidget             *widget,
                                      GtkOverlayLayoutChild *child,
                                      GtkAllocation         *widget_allocation)
{
  GtkAllocation allocation;
  gboolean result;

  g_signal_emit_by_name (overlay, "get-child-position",
                         widget, &allocation, &result);

  widget_allocation->x = allocation.x;
  widget_allocation->y = allocation.y;
  widget_allocation->width = allocation.width;
  widget_allocation->height = allocation.height;
}

static GtkAlign
effective_align (GtkAlign         align,
                 GtkTextDirection direction)
{
  switch (align)
    {
    case GTK_ALIGN_START:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_END : GTK_ALIGN_START;
    case GTK_ALIGN_END:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_START : GTK_ALIGN_END;
    case GTK_ALIGN_FILL:
    case GTK_ALIGN_CENTER:
    case GTK_ALIGN_BASELINE:
    default:
      return align;
    }
}

static void
gtk_overlay_child_update_style_classes (GtkOverlay *overlay,
                                        GtkWidget *child,
                                        GtkAllocation *child_allocation)
{
  int width, height;
  GtkAlign valign, halign;
  gboolean is_left, is_right, is_top, is_bottom;
  gboolean has_left, has_right, has_top, has_bottom;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (child);
  has_left = gtk_style_context_has_class (context, GTK_STYLE_CLASS_LEFT);
  has_right = gtk_style_context_has_class (context, GTK_STYLE_CLASS_RIGHT);
  has_top = gtk_style_context_has_class (context, GTK_STYLE_CLASS_TOP);
  has_bottom = gtk_style_context_has_class (context, GTK_STYLE_CLASS_BOTTOM);

  is_left = is_right = is_top = is_bottom = FALSE;

  width = gtk_widget_get_width (GTK_WIDGET (overlay));
  height = gtk_widget_get_height (GTK_WIDGET (overlay));

  halign = effective_align (gtk_widget_get_halign (child),
                            gtk_widget_get_direction (child));

  if (halign == GTK_ALIGN_START)
    is_left = (child_allocation->x == 0);
  else if (halign == GTK_ALIGN_END)
    is_right = (child_allocation->x + child_allocation->width == width);

  valign = gtk_widget_get_valign (child);

  if (valign == GTK_ALIGN_START)
    is_top = (child_allocation->y == 0);
  else if (valign == GTK_ALIGN_END)
    is_bottom = (child_allocation->y + child_allocation->height == height);

  if (has_left && !is_left)
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_LEFT);
  else if (!has_left && is_left)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_LEFT);

  if (has_right && !is_right)
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_RIGHT);
  else if (!has_right && is_right)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_RIGHT);

  if (has_top && !is_top)
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_TOP);
  else if (!has_top && is_top)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOP);

  if (has_bottom && !is_bottom)
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_BOTTOM);
  else if (!has_bottom && is_bottom)
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_BOTTOM);
}

static void
gtk_overlay_child_allocate (GtkOverlay            *overlay,
                            GtkWidget             *widget,
                            GtkOverlayLayoutChild *child)
{
  GtkAllocation child_allocation;

  if (!gtk_widget_should_layout (widget))
    return;

  gtk_overlay_compute_child_allocation (overlay, widget, child, &child_allocation);

  gtk_overlay_child_update_style_classes (overlay, widget, &child_allocation);
  gtk_widget_size_allocate (widget, &child_allocation, -1);
}

static void
gtk_overlay_layout_allocate (GtkLayoutManager *layout_manager,
                             GtkWidget        *widget,
                             int               width,
                             int               height,
                             int               baseline)
{
  GtkWidget *child;
  GtkWidget *main_widget;

  main_widget = gtk_bin_get_child (GTK_BIN (widget));
  if (main_widget && gtk_widget_get_visible (main_widget))
    gtk_widget_size_allocate (main_widget,
                              &(GtkAllocation) { 0, 0, width, height },
                              -1);

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (child != main_widget)
        {
          GtkOverlayLayoutChild *child_data;
          child_data = GTK_OVERLAY_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (layout_manager, child));

          gtk_overlay_child_allocate (GTK_OVERLAY (widget), child, child_data);
        }
    }
}

static GtkLayoutChild *
gtk_overlay_layout_create_layout_child (GtkLayoutManager *manager,
                                        GtkWidget        *widget,
                                        GtkWidget        *for_child)
{
  return g_object_new (GTK_TYPE_OVERLAY_LAYOUT_CHILD,
                       "layout-manager", manager,
                       "child-widget", for_child,
                       NULL);
}

static void
gtk_overlay_layout_class_init (GtkOverlayLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->measure = gtk_overlay_layout_measure;
  layout_class->allocate = gtk_overlay_layout_allocate;
  layout_class->create_layout_child = gtk_overlay_layout_create_layout_child;
}

static void
gtk_overlay_layout_init (GtkOverlayLayout *self)
{
}

GtkLayoutManager *
gtk_overlay_layout_new (void)
{
  return g_object_new (GTK_TYPE_OVERLAY_LAYOUT, NULL);
}
