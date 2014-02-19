/* gtkcellareacontext.c
 *
 * Copyright (C) 2010 Openismus GmbH
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:gtkcellareacontext
 * @Short_Description: Stores geometrical information for a series of rows in a GtkCellArea
 * @Title: GtkCellAreaContext
 *
 * The #GtkCellAreaContext object is created by a given #GtkCellArea
 * implementation via its #GtkCellAreaClass.create_context() virtual
 * method and is used to store cell sizes and alignments for a series of
 * #GtkTreeModel rows that are requested and rendered in the same context.
 *
 * #GtkCellLayout widgets can create any number of contexts in which to
 * request and render groups of data rows. However, it’s important that the
 * same context which was used to request sizes for a given #GtkTreeModel
 * row also be used for the same row when calling other #GtkCellArea APIs
 * such as gtk_cell_area_render() and gtk_cell_area_event().
 */

#include "config.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkcellareacontext.h"
#include "gtkprivate.h"

/* GObjectClass */
static void gtk_cell_area_context_dispose       (GObject            *object);
static void gtk_cell_area_context_get_property  (GObject            *object,
                                                 guint               prop_id,
                                                 GValue             *value,
                                                 GParamSpec         *pspec);
static void gtk_cell_area_context_set_property  (GObject            *object,
                                                 guint               prop_id,
                                                 const GValue       *value,
                                                 GParamSpec         *pspec);

/* GtkCellAreaContextClass */
static void gtk_cell_area_context_real_reset    (GtkCellAreaContext *context);
static void gtk_cell_area_context_real_allocate (GtkCellAreaContext *context,
                                                 gint                width,
                                                 gint                height);

struct _GtkCellAreaContextPrivate
{
  GtkCellArea *cell_area;

  gint         min_width;
  gint         nat_width;
  gint         min_height;
  gint         nat_height;
  gint         alloc_width;
  gint         alloc_height;
};

enum {
  PROP_0,
  PROP_CELL_AREA,
  PROP_MIN_WIDTH,
  PROP_NAT_WIDTH,
  PROP_MIN_HEIGHT,
  PROP_NAT_HEIGHT
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkCellAreaContext, gtk_cell_area_context, G_TYPE_OBJECT)

static void
gtk_cell_area_context_init (GtkCellAreaContext *context)
{
  context->priv = gtk_cell_area_context_get_instance_private (context);
}

static void
gtk_cell_area_context_class_init (GtkCellAreaContextClass *class)
{
  GObjectClass     *object_class = G_OBJECT_CLASS (class);

  /* GObjectClass */
  object_class->dispose      = gtk_cell_area_context_dispose;
  object_class->get_property = gtk_cell_area_context_get_property;
  object_class->set_property = gtk_cell_area_context_set_property;

  /* GtkCellAreaContextClass */
  class->reset    = gtk_cell_area_context_real_reset;
  class->allocate = gtk_cell_area_context_real_allocate;

  /**
   * GtkCellAreaContext:area:
   *
   * The #GtkCellArea this context was created by
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_CELL_AREA,
                                   g_param_spec_object ("area",
                                                        P_("Area"),
                                                        P_("The Cell Area this context was created for"),
                                                        GTK_TYPE_CELL_AREA,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * GtkCellAreaContext:minimum-width:
   *
   * The minimum width for the #GtkCellArea in this context
   * for all #GtkTreeModel rows that this context was requested
   * for using gtk_cell_area_get_preferred_width().
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_MIN_WIDTH,
                                   g_param_spec_int ("minimum-width",
                                                     P_("Minimum Width"),
                                                     P_("Minimum cached width"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READABLE));

  /**
   * GtkCellAreaContext:natural-width:
   *
   * The natural width for the #GtkCellArea in this context
   * for all #GtkTreeModel rows that this context was requested
   * for using gtk_cell_area_get_preferred_width().
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_NAT_WIDTH,
                                   g_param_spec_int ("natural-width",
                                                     P_("Minimum Width"),
                                                     P_("Minimum cached width"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READABLE));

  /**
   * GtkCellAreaContext:minimum-height:
   *
   * The minimum height for the #GtkCellArea in this context
   * for all #GtkTreeModel rows that this context was requested
   * for using gtk_cell_area_get_preferred_height().
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_MIN_HEIGHT,
                                   g_param_spec_int ("minimum-height",
                                                     P_("Minimum Height"),
                                                     P_("Minimum cached height"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READABLE));

  /**
   * GtkCellAreaContext:natural-height:
   *
   * The natural height for the #GtkCellArea in this context
   * for all #GtkTreeModel rows that this context was requested
   * for using gtk_cell_area_get_preferred_height().
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_NAT_HEIGHT,
                                   g_param_spec_int ("natural-height",
                                                     P_("Minimum Height"),
                                                     P_("Minimum cached height"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READABLE));
}

/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_context_dispose (GObject *object)
{
  GtkCellAreaContext        *context = GTK_CELL_AREA_CONTEXT (object);
  GtkCellAreaContextPrivate *priv = context->priv;

  if (priv->cell_area)
    {
      g_object_unref (priv->cell_area);

      priv->cell_area = NULL;
    }

  G_OBJECT_CLASS (gtk_cell_area_context_parent_class)->dispose (object);
}

static void
gtk_cell_area_context_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GtkCellAreaContext        *context = GTK_CELL_AREA_CONTEXT (object);
  GtkCellAreaContextPrivate *priv = context->priv;

  switch (prop_id)
    {
    case PROP_CELL_AREA:
      priv->cell_area = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_area_context_get_property (GObject     *object,
                                    guint        prop_id,
                                    GValue      *value,
                                    GParamSpec  *pspec)
{
  GtkCellAreaContext        *context = GTK_CELL_AREA_CONTEXT (object);
  GtkCellAreaContextPrivate *priv = context->priv;

  switch (prop_id)
    {
    case PROP_CELL_AREA:
      g_value_set_object (value, priv->cell_area);
      break;
    case PROP_MIN_WIDTH:
      g_value_set_int (value, priv->min_width);
      break;
    case PROP_NAT_WIDTH:
      g_value_set_int (value, priv->nat_width);
      break;
    case PROP_MIN_HEIGHT:
      g_value_set_int (value, priv->min_height);
      break;
    case PROP_NAT_HEIGHT:
      g_value_set_int (value, priv->nat_height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*************************************************************
 *                    GtkCellAreaContextClass                *
 *************************************************************/
static void
gtk_cell_area_context_real_reset (GtkCellAreaContext *context)
{
  GtkCellAreaContextPrivate *priv = context->priv;

  g_object_freeze_notify (G_OBJECT (context));

  if (priv->min_width != 0)
    {
      priv->min_width = 0;
      g_object_notify (G_OBJECT (context), "minimum-width");
    }

  if (priv->nat_width != 0)
    {
      priv->nat_width = 0;
      g_object_notify (G_OBJECT (context), "natural-width");
    }

  if (priv->min_height != 0)
    {
      priv->min_height = 0;
      g_object_notify (G_OBJECT (context), "minimum-height");
    }

  if (priv->nat_height != 0)
    {
      priv->nat_height = 0;
      g_object_notify (G_OBJECT (context), "natural-height");
    }

  priv->alloc_width  = 0;
  priv->alloc_height = 0;

  g_object_thaw_notify (G_OBJECT (context));
}

static void
gtk_cell_area_context_real_allocate (GtkCellAreaContext *context,
                                     gint                width,
                                     gint                height)
{
  GtkCellAreaContextPrivate *priv = context->priv;

  priv->alloc_width  = width;
  priv->alloc_height = height;
}

/*************************************************************
 *                            API                            *
 *************************************************************/
/**
 * gtk_cell_area_context_get_area:
 * @context: a #GtkCellAreaContext
 *
 * Fetches the #GtkCellArea this @context was created by.
 *
 * This is generally unneeded by layouting widgets; however,
 * it is important for the context implementation itself to
 * fetch information about the area it is being used for.
 *
 * For instance at #GtkCellAreaContextClass.allocate() time
 * it’s important to know details about any cell spacing
 * that the #GtkCellArea is configured with in order to
 * compute a proper allocation.
 *
 * Returns: (transfer none): the #GtkCellArea this context was created by.
 *
 * Since: 3.0
 */
GtkCellArea *
gtk_cell_area_context_get_area (GtkCellAreaContext *context)
{
  GtkCellAreaContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_AREA_CONTEXT (context), NULL);

  priv = context->priv;

  return priv->cell_area;
}

/**
 * gtk_cell_area_context_reset:
 * @context: a #GtkCellAreaContext
 *
 * Resets any previously cached request and allocation
 * data.
 *
 * When underlying #GtkTreeModel data changes its
 * important to reset the context if the content
 * size is allowed to shrink. If the content size
 * is only allowed to grow (this is usually an option
 * for views rendering large data stores as a measure
 * of optimization), then only the row that changed
 * or was inserted needs to be (re)requested with
 * gtk_cell_area_get_preferred_width().
 *
 * When the new overall size of the context requires
 * that the allocated size changes (or whenever this
 * allocation changes at all), the variable row
 * sizes need to be re-requested for every row.
 *
 * For instance, if the rows are displayed all with
 * the same width from top to bottom then a change
 * in the allocated width necessitates a recalculation
 * of all the displayed row heights using
 * gtk_cell_area_get_preferred_height_for_width().
 *
 * Since 3.0
 */
void
gtk_cell_area_context_reset (GtkCellAreaContext *context)
{
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  GTK_CELL_AREA_CONTEXT_GET_CLASS (context)->reset (context);
}

/**
 * gtk_cell_area_context_allocate:
 * @context: a #GtkCellAreaContext
 * @width: the allocated width for all #GtkTreeModel rows rendered
 *     with @context, or -1.
 * @height: the allocated height for all #GtkTreeModel rows rendered
 *     with @context, or -1.
 *
 * Allocates a width and/or a height for all rows which are to be
 * rendered with @context.
 *
 * Usually allocation is performed only horizontally or sometimes
 * vertically since a group of rows are usually rendered side by
 * side vertically or horizontally and share either the same width
 * or the same height. Sometimes they are allocated in both horizontal
 * and vertical orientations producing a homogeneous effect of the
 * rows. This is generally the case for #GtkTreeView when
 * #GtkTreeView:fixed-height-mode is enabled.
 *
 * Since 3.0
 */
void
gtk_cell_area_context_allocate (GtkCellAreaContext *context,
                                gint                width,
                                gint                height)
{
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  GTK_CELL_AREA_CONTEXT_GET_CLASS (context)->allocate (context, width, height);
}

/**
 * gtk_cell_area_context_get_preferred_width:
 * @context: a #GtkCellAreaContext
 * @minimum_width: (out) (allow-none): location to store the minimum width,
 *     or %NULL
 * @natural_width: (out) (allow-none): location to store the natural width,
 *     or %NULL
 *
 * Gets the accumulative preferred width for all rows which have been
 * requested with this context.
 *
 * After gtk_cell_area_context_reset() is called and/or before ever
 * requesting the size of a #GtkCellArea, the returned values are 0.
 *
 * Since: 3.0
 */
void
gtk_cell_area_context_get_preferred_width (GtkCellAreaContext *context,
                                           gint               *minimum_width,
                                           gint               *natural_width)
{
  GtkCellAreaContextPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  priv = context->priv;

  if (minimum_width)
    *minimum_width = priv->min_width;

  if (natural_width)
    *natural_width = priv->nat_width;
}

/**
 * gtk_cell_area_context_get_preferred_height:
 * @context: a #GtkCellAreaContext
 * @minimum_height: (out) (allow-none): location to store the minimum height,
 *     or %NULL
 * @natural_height: (out) (allow-none): location to store the natural height,
 *     or %NULL
 *
 * Gets the accumulative preferred height for all rows which have been
 * requested with this context.
 *
 * After gtk_cell_area_context_reset() is called and/or before ever
 * requesting the size of a #GtkCellArea, the returned values are 0.
 *
 * Since: 3.0
 */
void
gtk_cell_area_context_get_preferred_height (GtkCellAreaContext *context,
                                            gint               *minimum_height,
                                            gint               *natural_height)
{
  GtkCellAreaContextPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  priv = context->priv;

  if (minimum_height)
    *minimum_height = priv->min_height;

  if (natural_height)
    *natural_height = priv->nat_height;
}

/**
 * gtk_cell_area_context_get_preferred_height_for_width:
 * @context: a #GtkCellAreaContext
 * @width: a proposed width for allocation
 * @minimum_height: (out) (allow-none): location to store the minimum height,
 *     or %NULL
 * @natural_height: (out) (allow-none): location to store the natural height,
 *     or %NULL
 *
 * Gets the accumulative preferred height for @width for all rows
 * which have been requested for the same said @width with this context.
 *
 * After gtk_cell_area_context_reset() is called and/or before ever
 * requesting the size of a #GtkCellArea, the returned values are -1.
 *
 * Since: 3.0
 */
void
gtk_cell_area_context_get_preferred_height_for_width (GtkCellAreaContext *context,
                                                      gint                width,
                                                      gint               *minimum_height,
                                                      gint               *natural_height)
{
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  if (GTK_CELL_AREA_CONTEXT_GET_CLASS (context)->get_preferred_height_for_width)
    GTK_CELL_AREA_CONTEXT_GET_CLASS (context)->get_preferred_height_for_width (context,
                                                                               width,
                                                                               minimum_height,
                                                                               natural_height);
}

/**
 * gtk_cell_area_context_get_preferred_width_for_height:
 * @context: a #GtkCellAreaContext
 * @height: a proposed height for allocation
 * @minimum_width: (out) (allow-none): location to store the minimum width,
 *     or %NULL
 * @natural_width: (out) (allow-none): location to store the natural width,
 *     or %NULL
 *
 * Gets the accumulative preferred width for @height for all rows which
 * have been requested for the same said @height with this context.
 *
 * After gtk_cell_area_context_reset() is called and/or before ever
 * requesting the size of a #GtkCellArea, the returned values are -1.
 *
 * Since: 3.0
 */
void
gtk_cell_area_context_get_preferred_width_for_height (GtkCellAreaContext *context,
                                                      gint                height,
                                                      gint               *minimum_width,
                                                      gint               *natural_width)
{
  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  if (GTK_CELL_AREA_CONTEXT_GET_CLASS (context)->get_preferred_width_for_height)
    GTK_CELL_AREA_CONTEXT_GET_CLASS (context)->get_preferred_width_for_height (context,
                                                                               height,
                                                                               minimum_width,
                                                                               natural_width);
}

/**
 * gtk_cell_area_context_get_allocation:
 * @context: a #GtkCellAreaContext
 * @width: (out) (allow-none): location to store the allocated width, or %NULL
 * @height: (out) (allow-none): location to store the allocated height, or %NULL
 *
 * Fetches the current allocation size for @context.
 *
 * If the context was not allocated in width or height, or if the
 * context was recently reset with gtk_cell_area_context_reset(),
 * the returned value will be -1.
 *
 * Since: 3.0
 */
void
gtk_cell_area_context_get_allocation (GtkCellAreaContext *context,
                                      gint               *width,
                                      gint               *height)
{
  GtkCellAreaContextPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  priv = context->priv;

  if (width)
    *width = priv->alloc_width;

  if (height)
    *height = priv->alloc_height;
}

/**
 * gtk_cell_area_context_push_preferred_width:
 * @context: a #GtkCellAreaContext
 * @minimum_width: the proposed new minimum width for @context
 * @natural_width: the proposed new natural width for @context
 *
 * Causes the minimum and/or natural width to grow if the new
 * proposed sizes exceed the current minimum and natural width.
 *
 * This is used by #GtkCellAreaContext implementations during
 * the request process over a series of #GtkTreeModel rows to
 * progressively push the requested width over a series of
 * gtk_cell_area_get_preferred_width() requests.
 *
 * Since: 3.0
 */
void
gtk_cell_area_context_push_preferred_width (GtkCellAreaContext *context,
                                            gint                minimum_width,
                                            gint                natural_width)
{
  GtkCellAreaContextPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  priv = context->priv;

  g_object_freeze_notify (G_OBJECT (context));

  if (minimum_width > priv->min_width)
    {
      priv->min_width = minimum_width;

      g_object_notify (G_OBJECT (context), "minimum-width");
    }

  if (natural_width > priv->nat_width)
    {
      priv->nat_width = natural_width;

      g_object_notify (G_OBJECT (context), "natural-width");
    }

  g_object_thaw_notify (G_OBJECT (context));
}

/**
 * gtk_cell_area_context_push_preferred_height:
 * @context: a #GtkCellAreaContext
 * @minimum_height: the proposed new minimum height for @context
 * @natural_height: the proposed new natural height for @context
 *
 * Causes the minimum and/or natural height to grow if the new
 * proposed sizes exceed the current minimum and natural height.
 *
 * This is used by #GtkCellAreaContext implementations during
 * the request process over a series of #GtkTreeModel rows to
 * progressively push the requested height over a series of
 * gtk_cell_area_get_preferred_height() requests.
 *
 * Since: 3.0
 */
void
gtk_cell_area_context_push_preferred_height (GtkCellAreaContext *context,
                                             gint                minimum_height,
                                             gint                natural_height)
{
  GtkCellAreaContextPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_CONTEXT (context));

  priv = context->priv;

  g_object_freeze_notify (G_OBJECT (context));

  if (minimum_height > priv->min_height)
    {
      priv->min_height = minimum_height;

      g_object_notify (G_OBJECT (context), "minimum-height");
    }

  if (natural_height > priv->nat_height)
    {
      priv->nat_height = natural_height;

      g_object_notify (G_OBJECT (context), "natural-height");
    }

  g_object_thaw_notify (G_OBJECT (context));
}
