/*
 * gtkoverlay.c
 * This file is part of gtk
 *
 * Copyright (C) 2011 - Ignacio Casal Quinteiro, Mike Krüger
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

#include "gtkoverlay.h"
#include "gtkbuildable.h"
#include "gtkscrolledwindow.h"
#include "gtkwidgetprivate.h"
#include "gtkmarshalers.h"
#include "gtksnapshot.h"

#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkoverlay
 * @short_description: A container which overlays widgets on top of each other
 * @title: GtkOverlay
 *
 * GtkOverlay is a container which contains a single main child, on top
 * of which it can place “overlay” widgets. The position of each overlay
 * widget is determined by its #GtkWidget:halign and #GtkWidget:valign
 * properties. E.g. a widget with both alignments set to %GTK_ALIGN_START
 * will be placed at the top left corner of the GtkOverlay container,
 * whereas an overlay with halign set to %GTK_ALIGN_CENTER and valign set
 * to %GTK_ALIGN_END will be placed a the bottom edge of the GtkOverlay,
 * horizontally centered. The position can be adjusted by setting the margin
 * properties of the child to non-zero values.
 *
 * More complicated placement of overlays is possible by connecting
 * to the #GtkOverlay::get-child-position signal.
 *
 * # GtkOverlay as GtkBuildable
 *
 * The GtkOverlay implementation of the GtkBuildable interface
 * supports placing a child as an overlay by specifying “overlay” as
 * the “type” attribute of a `<child>` element.
 *
 * # CSS nodes
 *
 * GtkOverlay has a single CSS node with the name “overlay”. Overlay children
 * whose alignments cause them to be positioned at an edge get the style classes
 * “.left”, “.right”, “.top”, and/or “.bottom” according to their position.
 */
typedef struct _GtkOverlayChild GtkOverlayChild;

struct _GtkOverlayChild
{
  guint pass_through : 1;
  guint measure : 1;
  double blur;
};

enum {
  GET_CHILD_POSITION,
  LAST_SIGNAL
};

enum
{
  CHILD_PROP_0,
  CHILD_PROP_PASS_THROUGH,
  CHILD_PROP_MEASURE,
  CHILD_PROP_BLUR,
  CHILD_PROP_INDEX
};

static guint signals[LAST_SIGNAL] = { 0 };
static GQuark child_data_quark = 0;

static void gtk_overlay_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkOverlay, gtk_overlay, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_overlay_buildable_init))

static void
gtk_overlay_set_overlay_child (GtkWidget       *widget,
                               GtkOverlayChild *child_data)
{
  g_object_set_qdata_full (G_OBJECT (widget), child_data_quark, child_data, g_free);
}

static GtkOverlayChild *
gtk_overlay_get_overlay_child (GtkWidget *widget)
{
  return (GtkOverlayChild *) g_object_get_qdata (G_OBJECT (widget), child_data_quark);
}

static void
gtk_overlay_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkOverlayChild *child_data = gtk_overlay_get_overlay_child (child);

      if (child_data == NULL || child_data->measure)
        {
          int child_min, child_nat, child_min_baseline, child_nat_baseline;

          gtk_widget_measure (child,
                              orientation,
                              for_size,
                              &child_min, &child_nat,
                              &child_min_baseline, &child_nat_baseline);

          *minimum = MAX (*minimum, child_min);
          *natural = MAX (*natural, child_nat);
          if (child_min_baseline > -1)
            *minimum_baseline = MAX (*minimum_baseline, child_min_baseline);
          if (child_nat_baseline > -1)
            *natural_baseline = MAX (*natural_baseline, child_nat_baseline);
        }
    }
}

static void
gtk_overlay_compute_child_allocation (GtkOverlay      *overlay,
                                      GtkWidget       *widget,
                                      GtkOverlayChild *child,
                                      GtkAllocation   *widget_allocation)
{
  GtkAllocation allocation;
  gboolean result;

  g_signal_emit (overlay, signals[GET_CHILD_POSITION],
                 0, widget, &allocation, &result);

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
gtk_overlay_get_main_widget_allocation (GtkOverlay *overlay,
                                        GtkAllocation *main_alloc_out)
{
  GtkWidget *main_widget;
  GtkAllocation main_alloc;

  main_widget = gtk_bin_get_child (GTK_BIN (overlay));

  /* special-case scrolled windows */
  if (GTK_IS_SCROLLED_WINDOW (main_widget))
    {
      GtkWidget *grandchild;
      gint x, y;
      gboolean res;

      grandchild = gtk_bin_get_child (GTK_BIN (main_widget));
      res = gtk_widget_translate_coordinates (grandchild, main_widget, 0, 0, &x, &y);

      if (res)
        {
          main_alloc.x = x;
          main_alloc.y = y;
        }
      else
        {
          main_alloc.x = 0;
          main_alloc.y = 0;
        }

      main_alloc.width = gtk_widget_get_allocated_width (grandchild);
      main_alloc.height = gtk_widget_get_allocated_height (grandchild);
    }
  else
    {
      main_alloc.x = 0;
      main_alloc.y = 0;
      main_alloc.width = gtk_widget_get_allocated_width (GTK_WIDGET (overlay));
      main_alloc.height = gtk_widget_get_allocated_height (GTK_WIDGET (overlay));
    }

  *main_alloc_out = main_alloc;
}

static void
gtk_overlay_child_update_style_classes (GtkOverlay *overlay,
                                        GtkWidget *child,
                                        GtkAllocation *child_allocation)
{
  GtkAllocation main_allocation;
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

  gtk_overlay_get_main_widget_allocation (overlay, &main_allocation);

  halign = effective_align (gtk_widget_get_halign (child),
                            gtk_widget_get_direction (child));

  if (halign == GTK_ALIGN_START)
    is_left = (child_allocation->x == main_allocation.x);
  else if (halign == GTK_ALIGN_END)
    is_right = (child_allocation->x + child_allocation->width ==
                main_allocation.x + main_allocation.width);

  valign = gtk_widget_get_valign (child);

  if (valign == GTK_ALIGN_START)
    is_top = (child_allocation->y == main_allocation.y);
  else if (valign == GTK_ALIGN_END)
    is_bottom = (child_allocation->y + child_allocation->height ==
                 main_allocation.y + main_allocation.height);

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
gtk_overlay_child_allocate (GtkOverlay      *overlay,
                            GtkWidget       *widget,
                            GtkOverlayChild *child)
{
  GtkAllocation child_allocation;

  if (!gtk_widget_get_visible (widget))
    return;

  gtk_overlay_compute_child_allocation (overlay, widget, child, &child_allocation);

  gtk_overlay_child_update_style_classes (overlay, widget, &child_allocation);
  gtk_widget_size_allocate (widget, &child_allocation, -1);
}

static void
gtk_overlay_size_allocate (GtkWidget           *widget,
                           const GtkAllocation *allocation,
                           int                  baseline)
{
  GtkOverlay *overlay = GTK_OVERLAY (widget);
  GtkWidget *child;
  GtkWidget *main_widget;

  main_widget = gtk_bin_get_child (GTK_BIN (overlay));
  if (main_widget && gtk_widget_get_visible (main_widget))
    gtk_widget_size_allocate (main_widget, allocation, -1);

  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (child != main_widget)
        {
          GtkOverlayChild *child_data = gtk_overlay_get_overlay_child (child);
          gtk_overlay_child_allocate (overlay, child, child_data);
        }
    }
}

static gboolean
gtk_overlay_get_child_position (GtkOverlay    *overlay,
                                GtkWidget     *widget,
                                GtkAllocation *alloc)
{
  GtkAllocation main_alloc;
  GtkRequisition min, req;
  GtkAlign halign;
  GtkTextDirection direction;

  gtk_overlay_get_main_widget_allocation (overlay, &main_alloc);
  gtk_widget_get_preferred_size (widget, &min, &req);

  alloc->x = main_alloc.x;
  alloc->width = MAX (min.width, MIN (main_alloc.width, req.width));

  direction = _gtk_widget_get_direction (widget);

  halign = gtk_widget_get_halign (widget);
  switch (effective_align (halign, direction))
    {
    case GTK_ALIGN_START:
      /* nothing to do */
      break;
    case GTK_ALIGN_FILL:
      alloc->width = MAX (alloc->width, main_alloc.width);
      break;
    case GTK_ALIGN_CENTER:
      alloc->x += main_alloc.width / 2 - alloc->width / 2;
      break;
    case GTK_ALIGN_END:
      alloc->x += main_alloc.width - alloc->width;
      break;
    case GTK_ALIGN_BASELINE:
    default:
      g_assert_not_reached ();
      break;
    }

  alloc->y = main_alloc.y;
  alloc->height = MAX  (min.height, MIN (main_alloc.height, req.height));

  switch (gtk_widget_get_valign (widget))
    {
    case GTK_ALIGN_START:
      /* nothing to do */
      break;
    case GTK_ALIGN_FILL:
      alloc->height = MAX (alloc->height, main_alloc.height);
      break;
    case GTK_ALIGN_CENTER:
      alloc->y += main_alloc.height / 2 - alloc->height / 2;
      break;
    case GTK_ALIGN_END:
      alloc->y += main_alloc.height - alloc->height;
      break;
    case GTK_ALIGN_BASELINE:
    default:
      g_assert_not_reached ();
      break;
    }

  return TRUE;
}

static void
gtk_overlay_add (GtkContainer *container,
                 GtkWidget    *widget)
{

  /* We only get into this path if we have no child
   * (since GtkOverlay is a GtkBin) and the only child
   * we can add through gtk_container_add is the main child,
   * which we want to keep the lowest in the hierarchy. */
  gtk_widget_insert_after (widget, GTK_WIDGET (container), NULL);
  _gtk_bin_set_child (GTK_BIN (container), widget);
}

static void
gtk_overlay_remove (GtkContainer *container,
                    GtkWidget    *widget)
{
  GtkWidget *main_child = gtk_bin_get_child (GTK_BIN (container));

  if (widget == main_child)
    {
      GTK_CONTAINER_CLASS (gtk_overlay_parent_class)->remove (container, widget);
    }
  else
    {
      GtkWidget *w;

      gtk_widget_unparent (widget);

      for (w = gtk_widget_get_first_child (GTK_WIDGET (container));
           w != NULL;
           w = gtk_widget_get_next_sibling (w))
        {
          gtk_widget_child_notify (w, "index");
        }
    }
}

/**
 * gtk_overlay_reorder_overlay:
 * @overlay: a #GtkOverlay
 * @child: the overlaid #GtkWidget to move
 * @position: the new index for @child in the list of overlay children
 *   of @overlay, starting from 0. If negative, indicates the end of
 *   the list
 *
 * Moves @child to a new @index in the list of @overlay children.
 * The list contains overlays in the order that these were
 * added to @overlay.
 *
 * A widget’s index in the @overlay children list determines which order
 * the children are drawn if they overlap. The first child is drawn at
 * the bottom. It also affects the default focus chain order.
 */
void
gtk_overlay_reorder_overlay (GtkOverlay *overlay,
                             GtkWidget  *child,
                             gint        position)
{
  GtkWidget *w;

  g_return_if_fail (GTK_IS_OVERLAY (overlay));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (gtk_widget_get_parent (child) == GTK_WIDGET (overlay));

  if (child == gtk_bin_get_child (GTK_BIN (overlay)))
    return;

  if (position < 0)
    {
      /* Just move it to the end */
      gtk_widget_insert_before (child, GTK_WIDGET (overlay), NULL);
    }
  else
    {
      int pos = 0;
      for (w = gtk_widget_get_first_child (GTK_WIDGET (overlay));
           w != NULL;
           w = gtk_widget_get_next_sibling (w))
        {
          if (pos == position)
            break;

          pos ++;
        }

      if (w == child)
        return;

      gtk_widget_insert_after (child, GTK_WIDGET (overlay), w);
    }

  /* Not all indices changed, but notify for all of them, for simplicity. */
  for (w = gtk_widget_get_first_child (GTK_WIDGET (overlay));
       w != NULL;
       w = gtk_widget_get_next_sibling (w))
    {
      gtk_widget_child_notify (w, "index");
    }
}


static void
gtk_overlay_forall (GtkContainer *overlay,
                    GtkCallback   callback,
                    gpointer      callback_data)
{
  GtkWidget *child;

  child = gtk_widget_get_first_child (GTK_WIDGET (overlay));
  while (child != NULL)
    {
      GtkWidget *next = gtk_widget_get_next_sibling (child);

      (* callback) (child, callback_data);

      child = next;
    }
}


static void
gtk_overlay_set_child_property (GtkContainer *container,
				GtkWidget    *child,
				guint         property_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  GtkOverlay *overlay = GTK_OVERLAY (container);
  GtkOverlayChild *child_info;
  GtkWidget *main_widget;

  main_widget = gtk_bin_get_child (GTK_BIN (overlay));
  if (child == main_widget)
    child_info = NULL;
  else
    {
      child_info = gtk_overlay_get_overlay_child (child);
      if (child_info == NULL)
	{
	  GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
	  return;
	}
    }

  switch (property_id)
    {
    case CHILD_PROP_PASS_THROUGH:
      /* Ignore value on main child */
      if (child_info)
	{
	  if (g_value_get_boolean (value) != child_info->pass_through)
	    {
	      child_info->pass_through = g_value_get_boolean (value);
              gtk_widget_set_pass_through (child, child_info->pass_through);
	      gtk_container_child_notify (container, child, "pass-through");
	    }
	}
      break;
    case CHILD_PROP_MEASURE:
      if (child_info)
	{
	  if (g_value_get_boolean (value) != child_info->measure)
	    {
	      child_info->measure = g_value_get_boolean (value);
	      gtk_container_child_notify (container, child, "measure");
              gtk_widget_queue_resize (GTK_WIDGET (overlay));
	    }
	}
      break;
    case CHILD_PROP_BLUR:
      if (child_info)
	{
	  if (g_value_get_double (value) != child_info->blur)
	    {
	      child_info->blur = g_value_get_double (value);
	      gtk_container_child_notify (container, child, "blur");
              gtk_widget_queue_draw (GTK_WIDGET (overlay));
	    }
	}
      break;
    case CHILD_PROP_INDEX:
      if (child_info != NULL)
	gtk_overlay_reorder_overlay (GTK_OVERLAY (container),
				     child,
				     g_value_get_int (value));
      break;

    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_overlay_get_child_property (GtkContainer *container,
				GtkWidget    *child,
				guint         property_id,
				GValue       *value,
				GParamSpec   *pspec)
{
  GtkOverlay *overlay = GTK_OVERLAY (container);
  GtkOverlayChild *child_info;
  GtkWidget *main_widget;
  GtkWidget *w;
  int pos = 0;

  main_widget = gtk_bin_get_child (GTK_BIN (overlay));
  if (child == main_widget)
    child_info = NULL;
  else
    {
      child_info = gtk_overlay_get_overlay_child (child);
      if (child_info == NULL)
	{
	  GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
	  return;
	}
    }

  switch (property_id)
    {
    case CHILD_PROP_PASS_THROUGH:
      if (child_info)
	g_value_set_boolean (value, child_info->pass_through);
      else
	g_value_set_boolean (value, FALSE);
      break;
    case CHILD_PROP_MEASURE:
      if (child_info)
	g_value_set_boolean (value, child_info->measure);
      else
	g_value_set_boolean (value, TRUE);
      break;
    case CHILD_PROP_BLUR:
      if (child_info)
	g_value_set_double (value, child_info->blur);
      else
	g_value_set_double (value, 0);
      break;
    case CHILD_PROP_INDEX:
      for (w = _gtk_widget_get_first_child (GTK_WIDGET (container));
           w != child;
           w = _gtk_widget_get_next_sibling (w))
        {
          pos ++;
        }

      g_value_set_int (value, pos);
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_overlay_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkWidget *main_widget;
  GskRenderNode *main_widget_node = NULL;
  GtkWidget *child;
  GtkAllocation main_alloc;
  cairo_region_t *clip = NULL;
  int i;

  main_widget = gtk_bin_get_child (GTK_BIN (widget));
  gtk_widget_get_allocation (widget, &main_alloc);

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      double blur;
      gtk_container_child_get (GTK_CONTAINER (widget), child, "blur", &blur, NULL);
      if (blur > 0)
        {
          GtkAllocation alloc;
          graphene_rect_t bounds;

          if (main_widget_node == NULL)
            {
              GtkSnapshot *child_snapshot;

              child_snapshot = gtk_snapshot_new (gtk_snapshot_get_record_names (snapshot),
                                                 "OverlayCaptureMainChild");
              gtk_snapshot_offset (child_snapshot, main_alloc.x, main_alloc.y);
              gtk_widget_snapshot (main_widget, child_snapshot);
              gtk_snapshot_offset (child_snapshot, -main_alloc.x, -main_alloc.y);
              main_widget_node = gtk_snapshot_free_to_node (child_snapshot);
            }

          gtk_widget_get_allocation (child, &alloc);
          graphene_rect_init (&bounds, alloc.x, alloc.y, alloc.width, alloc.height);
          gtk_snapshot_push_clip (snapshot, &bounds, "Overlay Effect Clip");
          gtk_snapshot_push_blur (snapshot, blur, "Overlay Effect");
          gtk_snapshot_append_node (snapshot, main_widget_node);
          gtk_snapshot_pop (snapshot);
          gtk_snapshot_pop (snapshot);

          if (clip == NULL)
            {
              cairo_rectangle_int_t rect;
              rect.x = rect.y = 0;
              rect.width = main_alloc.width;
              rect.height = main_alloc.height;
              clip = cairo_region_create ();
              cairo_region_union_rectangle (clip, &rect);
            }
          cairo_region_subtract_rectangle (clip, (cairo_rectangle_int_t *)&alloc);
        }
    }

  if (clip == NULL)
    {
      GTK_WIDGET_CLASS (gtk_overlay_parent_class)->snapshot (widget, snapshot);
      return;
    }

  for (i = 0; i < cairo_region_num_rectangles (clip); i++)
    {
      cairo_rectangle_int_t rect;
      graphene_rect_t bounds;

      cairo_region_get_rectangle (clip, i, &rect);
      graphene_rect_init (&bounds, rect.x, rect.y, rect.width, rect.height);
      gtk_snapshot_push_clip (snapshot, &bounds, "Overlay Non-Effect Clip");
      gtk_snapshot_append_node (snapshot, main_widget_node);
      gtk_snapshot_pop (snapshot);
    }

  cairo_region_destroy (clip);

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (child != main_widget)
        gtk_widget_snapshot_child (widget, child, snapshot);
    }

  gsk_render_node_unref (main_widget_node);
}

static void
gtk_overlay_class_init (GtkOverlayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  widget_class->measure = gtk_overlay_measure;
  widget_class->size_allocate = gtk_overlay_size_allocate;
  widget_class->snapshot = gtk_overlay_snapshot;

  container_class->add = gtk_overlay_add;
  container_class->remove = gtk_overlay_remove;
  container_class->forall = gtk_overlay_forall;
  container_class->set_child_property = gtk_overlay_set_child_property;
  container_class->get_child_property = gtk_overlay_get_child_property;

  klass->get_child_position = gtk_overlay_get_child_position;

  /**
   * GtkOverlay:pass-through:
   *
   * Pass through input, does not affect main child.
   */
  gtk_container_class_install_child_property (container_class, CHILD_PROP_PASS_THROUGH,
      g_param_spec_boolean ("pass-through", P_("Pass Through"), P_("Pass through input, does not affect main child"),
                            FALSE,
                            GTK_PARAM_READWRITE));

  /**
   * GtkOverlay:measure:
   *
   * Include this child in determining the child request.
   *
   * The main child will always be measured.
   */
  gtk_container_class_install_child_property (container_class, CHILD_PROP_MEASURE,
      g_param_spec_boolean ("measure", P_("Measure"), P_("Include in size measurement"),
                            FALSE,
                            GTK_PARAM_READWRITE));

  /**
   * GtkOverlay:blur:
   *
   * Blur the content behind this child with a Gaussian blur of this radius.
   */
  gtk_container_class_install_child_property (container_class, CHILD_PROP_BLUR,
      g_param_spec_double ("blur", P_("Blur Radius"), P_("Apply a blur to the content behind this child"),
                           0, 100, 0,
                           GTK_PARAM_READWRITE));
  /**
   * GtkOverlay:index:
   *
   * The index of the overlay in the parent, -1 for the main child.
   */
  gtk_container_class_install_child_property (container_class, CHILD_PROP_INDEX,
					      g_param_spec_int ("index",
								P_("Index"),
								P_("The index of the overlay in the parent, -1 for the main child"),
								-1, G_MAXINT, 0,
								GTK_PARAM_READWRITE));

  /**
   * GtkOverlay::get-child-position:
   * @overlay: the #GtkOverlay
   * @widget: the child widget to position
   * @allocation: (type Gdk.Rectangle) (out caller-allocates): return
   *   location for the allocation
   *
   * The ::get-child-position signal is emitted to determine
   * the position and size of any overlay child widgets. A
   * handler for this signal should fill @allocation with
   * the desired position and size for @widget, relative to
   * the 'main' child of @overlay.
   *
   * The default handler for this signal uses the @widget's
   * halign and valign properties to determine the position
   * and gives the widget its natural size (except that an
   * alignment of %GTK_ALIGN_FILL will cause the overlay to
   * be full-width/height). If the main child is a
   * #GtkScrolledWindow, the overlays are placed relative
   * to its contents.
   *
   * Returns: %TRUE if the @allocation has been filled
   */
  signals[GET_CHILD_POSITION] =
    g_signal_new (I_("get-child-position"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkOverlayClass, get_child_position),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__OBJECT_BOXED,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_WIDGET,
                  GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);

  child_data_quark = g_quark_from_static_string ("gtk-overlay-child-data");

  gtk_widget_class_set_css_name (widget_class, I_("overlay"));
}

static void
gtk_overlay_init (GtkOverlay *overlay)
{
  gtk_widget_set_has_surface (GTK_WIDGET (overlay), FALSE);
}

static void
gtk_overlay_buildable_add_child (GtkBuildable *buildable,
                                 GtkBuilder   *builder,
                                 GObject      *child,
                                 const gchar  *type)
{
  if (type && strcmp (type, "overlay") == 0)
    gtk_overlay_add_overlay (GTK_OVERLAY (buildable), GTK_WIDGET (child));
  else if (!type)
    {
      /* Make sure the main-child node is the first one */
      gtk_widget_insert_after (GTK_WIDGET (child), GTK_WIDGET (buildable), NULL);
      _gtk_bin_set_child (GTK_BIN (buildable), GTK_WIDGET (child));
    }
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
}

static void
gtk_overlay_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_overlay_buildable_add_child;
}

/**
 * gtk_overlay_new:
 *
 * Creates a new #GtkOverlay.
 *
 * Returns: a new #GtkOverlay object.
 */
GtkWidget *
gtk_overlay_new (void)
{
  return g_object_new (GTK_TYPE_OVERLAY, NULL);
}

/**
 * gtk_overlay_add_overlay:
 * @overlay: a #GtkOverlay
 * @widget: a #GtkWidget to be added to the container
 *
 * Adds @widget to @overlay.
 *
 * The widget will be stacked on top of the main widget
 * added with gtk_container_add().
 *
 * The position at which @widget is placed is determined
 * from its #GtkWidget:halign and #GtkWidget:valign properties.
 */
void
gtk_overlay_add_overlay (GtkOverlay *overlay,
                         GtkWidget  *widget)
{
  GtkOverlayChild *child;

  g_return_if_fail (GTK_IS_OVERLAY (overlay));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  child = g_new0 (GtkOverlayChild, 1);

  gtk_widget_insert_before (widget, GTK_WIDGET (overlay), NULL);
  gtk_overlay_set_overlay_child (widget, child);

  gtk_widget_child_notify (widget, "index");
}

/**
 * gtk_overlay_set_overlay_pass_through:
 * @overlay: a #GtkOverlay
 * @widget: an overlay child of #GtkOverlay
 * @pass_through: whether the child should pass the input through
 *
 * Convenience function to set the value of the #GtkOverlay:pass-through
 * child property for @widget.
 */
void
gtk_overlay_set_overlay_pass_through (GtkOverlay *overlay,
				      GtkWidget  *widget,
				      gboolean    pass_through)
{
  g_return_if_fail (GTK_IS_OVERLAY (overlay));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_container_child_set (GTK_CONTAINER (overlay), widget,
			   "pass-through", pass_through,
			   NULL);
}

/**
 * gtk_overlay_get_overlay_pass_through:
 * @overlay: a #GtkOverlay
 * @widget: an overlay child of #GtkOverlay
 *
 * Convenience function to get the value of the #GtkOverlay:pass-through
 * child property for @widget.
 *
 * Returns: whether the widget is a pass through child.
 */
gboolean
gtk_overlay_get_overlay_pass_through (GtkOverlay *overlay,
				      GtkWidget  *widget)
{
  gboolean pass_through;

  g_return_val_if_fail (GTK_IS_OVERLAY (overlay), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  gtk_container_child_get (GTK_CONTAINER (overlay), widget,
			   "pass-through", &pass_through,
			   NULL);

  return pass_through;
}

/**
 * gtk_overlay_set_measure_overlay:
 * @overlay: a #GtkOverlay
 * @widget: an overlay child of #GtkOverlay
 * @measure: whether the child should be measured
 *
 * Sets whether @widget is included in the measured size of @overlay.
 *
 * The overlay will request the size of the largest child that has
 * this property set to %TRUE. Children who are not included may
 * be drawn outside of @overlay's allocation if they are too large.
 */
void
gtk_overlay_set_measure_overlay (GtkOverlay *overlay,
				 GtkWidget  *widget,
				 gboolean    measure)
{
  g_return_if_fail (GTK_IS_OVERLAY (overlay));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_container_child_set (GTK_CONTAINER (overlay), widget,
			   "measure", measure,
			   NULL);
}

/**
 * gtk_overlay_get_measure_overlay:
 * @overlay: a #GtkOverlay
 * @widget: an overlay child of #GtkOverlay
 *
 * Gets whether @widget's size is included in the measurement of
 * @overlay.
 *
 * Returns: whether the widget is measured
 */
gboolean
gtk_overlay_get_measure_overlay (GtkOverlay *overlay,
				 GtkWidget  *widget)
{
  gboolean measure;

  g_return_val_if_fail (GTK_IS_OVERLAY (overlay), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  gtk_container_child_get (GTK_CONTAINER (overlay), widget,
			   "measure", &measure,
			   NULL);

  return measure;
}
