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

#include "gtkoverlaylayoutprivate.h"
#include "gtkbuildable.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkscrolledwindow.h"
#include "gtksnapshot.h"
#include "gtkstylecontext.h"
#include "gtkwidgetprivate.h"

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
 * An overlay’s minimum and natural sizes are those of its main child. The sizes
 * of overlay children are not considered when measuring these preferred sizes.
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

enum {
  GET_CHILD_POSITION,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void gtk_overlay_buildable_init (GtkBuildableIface *iface);

typedef struct _GtkOverlayClass    GtkOverlayClass;

struct _GtkOverlay
{
  GtkBin parent_instance;
};

struct _GtkOverlayClass
{
  GtkBinClass parent_class;

  gboolean (*get_child_position) (GtkOverlay    *overlay,
                                  GtkWidget     *widget,
                                  GtkAllocation *allocation);
};

typedef struct {
  GtkLayoutManager *layout;
} GtkOverlayPrivate;

G_DEFINE_TYPE_WITH_CODE (GtkOverlay, gtk_overlay, GTK_TYPE_BIN,
                         G_ADD_PRIVATE (GtkOverlay)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_overlay_buildable_init))

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

static gboolean
gtk_overlay_get_child_position (GtkOverlay    *overlay,
                                GtkWidget     *widget,
                                GtkAllocation *alloc)
{
  GtkRequisition min, req;
  GtkAlign halign;
  GtkTextDirection direction;
  int width, height;

  gtk_widget_get_preferred_size (widget, &min, &req);
  width = gtk_widget_get_width (GTK_WIDGET (overlay));
  height = gtk_widget_get_height (GTK_WIDGET (overlay));

  alloc->x = 0;
  alloc->width = MAX (min.width, MIN (width, req.width));

  direction = _gtk_widget_get_direction (widget);

  halign = gtk_widget_get_halign (widget);
  switch (effective_align (halign, direction))
    {
    case GTK_ALIGN_START:
      /* nothing to do */
      break;
    case GTK_ALIGN_FILL:
      alloc->width = MAX (alloc->width, width);
      break;
    case GTK_ALIGN_CENTER:
      alloc->x += width / 2 - alloc->width / 2;
      break;
    case GTK_ALIGN_END:
      alloc->x += width - alloc->width;
      break;
    case GTK_ALIGN_BASELINE:
    default:
      g_assert_not_reached ();
      break;
    }

  alloc->y = 0;
  alloc->height = MAX  (min.height, MIN (height, req.height));

  switch (gtk_widget_get_valign (widget))
    {
    case GTK_ALIGN_START:
      /* nothing to do */
      break;
    case GTK_ALIGN_FILL:
      alloc->height = MAX (alloc->height, height);
      break;
    case GTK_ALIGN_CENTER:
      alloc->y += height / 2 - alloc->height / 2;
      break;
    case GTK_ALIGN_END:
      alloc->y += height - alloc->height;
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
   * which we want to keep the lowest in the hierarchy.
   */
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
      gtk_widget_unparent (widget);
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
gtk_overlay_snapshot_child (GtkWidget   *overlay,
                            GtkWidget   *child,
                            GtkSnapshot *snapshot)
{
  graphene_rect_t bounds;
  gboolean clip_set;

  clip_set = gtk_overlay_get_clip_overlay (GTK_OVERLAY (overlay), child);

  if (!clip_set)
    {
      gtk_widget_snapshot_child (overlay, child, snapshot);
      return;
    }

  graphene_rect_init (&bounds, 0, 0,
                      gtk_widget_get_width (overlay),
                      gtk_widget_get_height (overlay));

  gtk_snapshot_push_clip (snapshot, &bounds);
  gtk_widget_snapshot_child (overlay, child, snapshot);
  gtk_snapshot_pop (snapshot);
}

static void
gtk_overlay_snapshot (GtkWidget   *widget,
                      GtkSnapshot *snapshot)
{
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      gtk_overlay_snapshot_child (widget, child, snapshot);
    }
}

static void
gtk_overlay_class_init (GtkOverlayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  widget_class->snapshot = gtk_overlay_snapshot;

  container_class->add = gtk_overlay_add;
  container_class->remove = gtk_overlay_remove;
  container_class->forall = gtk_overlay_forall;

  klass->get_child_position = gtk_overlay_get_child_position;

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
  g_signal_set_va_marshaller (signals[GET_CHILD_POSITION],
                              G_TYPE_FROM_CLASS (object_class),
                              _gtk_marshal_BOOLEAN__OBJECT_BOXEDv);

  gtk_widget_class_set_css_name (widget_class, I_("overlay"));

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_OVERLAY_LAYOUT);
}

static void
gtk_overlay_init (GtkOverlay *overlay)
{
  GtkOverlayPrivate *priv = gtk_overlay_get_instance_private (overlay);

  priv->layout = gtk_widget_get_layout_manager (GTK_WIDGET (overlay));
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_overlay_buildable_add_child (GtkBuildable *buildable,
                                 GtkBuilder   *builder,
                                 GObject      *child,
                                 const gchar  *type)
{
  if (GTK_IS_WIDGET (child))
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
  else
    {
      parent_buildable_iface->add_child (buildable, builder, child, type);
    }
}

static void
gtk_overlay_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

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
  g_return_if_fail (GTK_IS_OVERLAY (overlay));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_insert_before (widget, GTK_WIDGET (overlay), NULL);
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
  GtkOverlayPrivate *priv = gtk_overlay_get_instance_private (overlay);
  GtkOverlayLayoutChild *child;

  g_return_if_fail (GTK_IS_OVERLAY (overlay));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  child = GTK_OVERLAY_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));
  gtk_overlay_layout_child_set_measure (child, measure);
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
  GtkOverlayPrivate *priv = gtk_overlay_get_instance_private (overlay);
  GtkOverlayLayoutChild *child;

  g_return_val_if_fail (GTK_IS_OVERLAY (overlay), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  child = GTK_OVERLAY_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));
  return gtk_overlay_layout_child_get_measure (child);
}

/**
 * gtk_overlay_set_clip_overlay:
 * @overlay: a #GtkOverlay
 * @widget: an overlay child of #GtkOverlay
 * @clip_overlay: whether the child should be clipped
 *
 * Convenience function to set the value of the #GtkOverlay:clip-overlay
 * child property for @widget.
 */
void
gtk_overlay_set_clip_overlay (GtkOverlay *overlay,
                              GtkWidget  *widget,
                              gboolean    clip_overlay)
{
  GtkOverlayPrivate *priv = gtk_overlay_get_instance_private (overlay);
  GtkOverlayLayoutChild *child;

  g_return_if_fail (GTK_IS_OVERLAY (overlay));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  child = GTK_OVERLAY_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));
  gtk_overlay_layout_child_set_clip_overlay (child, clip_overlay);
}

/**
 * gtk_overlay_get_clip_overlay:
 * @overlay: a #GtkOverlay
 * @widget: an overlay child of #GtkOverlay
 *
 * Convenience function to get the value of the #GtkOverlay:clip-overlay
 * child property for @widget.
 *
 * Returns: whether the widget is clipped within the parent.
 */
gboolean
gtk_overlay_get_clip_overlay (GtkOverlay *overlay,
                              GtkWidget  *widget)
{
  GtkOverlayPrivate *priv = gtk_overlay_get_instance_private (overlay);
  GtkOverlayLayoutChild *child;

  g_return_val_if_fail (GTK_IS_OVERLAY (overlay), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  child = GTK_OVERLAY_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (priv->layout, widget));

  return gtk_overlay_layout_child_get_clip_overlay (child);
}
