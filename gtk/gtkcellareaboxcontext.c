/* gtkcellareaboxcontext.c
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gtkintl.h"
#include "gtkcellareabox.h"
#include "gtkcellareaboxcontext.h"
#include "gtkorientable.h"

/* GObjectClass */
static void      gtk_cell_area_box_context_finalize                         (GObject            *object);

/* GtkCellAreaContextClass */
static void      gtk_cell_area_box_context_flush_preferred_width            (GtkCellAreaContext *context);
static void      gtk_cell_area_box_context_flush_preferred_height           (GtkCellAreaContext *context);
static void      gtk_cell_area_box_context_flush_allocation                 (GtkCellAreaContext *context);
static void      gtk_cell_area_box_context_sum_preferred_width              (GtkCellAreaContext *context);
static void      gtk_cell_area_box_context_sum_preferred_height             (GtkCellAreaContext *context);
static void      gtk_cell_area_box_context_allocate_width                   (GtkCellAreaContext *context,
									     gint                width);
static void      gtk_cell_area_box_context_allocate_height                  (GtkCellAreaContext *context,
									     gint                height);

typedef struct {
  gint     min_size;
  gint     nat_size;
  gboolean expand;
} BaseSize;

struct _GtkCellAreaBoxContextPrivate
{
  /* Table of per renderer CachedSizes */
  GArray *base_widths;
  GArray *base_heights;

  /* Allocation info for this context if any */
  gint                      alloc_width;
  gint                      alloc_height;
  gint                      n_orientation_allocs;
  GtkCellAreaBoxAllocation *orientation_allocs;
};

G_DEFINE_TYPE (GtkCellAreaBoxContext, gtk_cell_area_box_context, GTK_TYPE_CELL_AREA_CONTEXT);

static void
gtk_cell_area_box_context_init (GtkCellAreaBoxContext *box_context)
{
  GtkCellAreaBoxContextPrivate *priv;

  box_context->priv = G_TYPE_INSTANCE_GET_PRIVATE (box_context,
						   GTK_TYPE_CELL_AREA_BOX_CONTEXT,
						   GtkCellAreaBoxContextPrivate);
  priv = box_context->priv;

  priv->base_widths  = g_array_new (FALSE, TRUE, sizeof (BaseSize));
  priv->base_heights = g_array_new (FALSE, TRUE, sizeof (BaseSize));

  priv->alloc_width  = 0;
  priv->alloc_height = 0;
  priv->orientation_allocs   = NULL;
  priv->n_orientation_allocs = 0;
}

static void 
gtk_cell_area_box_context_class_init (GtkCellAreaBoxContextClass *class)
{
  GObjectClass            *object_class = G_OBJECT_CLASS (class);
  GtkCellAreaContextClass *context_class   = GTK_CELL_AREA_CONTEXT_CLASS (class);

  /* GObjectClass */
  object_class->finalize = gtk_cell_area_box_context_finalize;

  context_class->flush_preferred_width            = gtk_cell_area_box_context_flush_preferred_width;
  context_class->flush_preferred_height           = gtk_cell_area_box_context_flush_preferred_height;
  context_class->flush_allocation                 = gtk_cell_area_box_context_flush_allocation;

  context_class->sum_preferred_width            = gtk_cell_area_box_context_sum_preferred_width;
  context_class->sum_preferred_height           = gtk_cell_area_box_context_sum_preferred_height;

  context_class->allocate_width  = gtk_cell_area_box_context_allocate_width;
  context_class->allocate_height = gtk_cell_area_box_context_allocate_height;

  g_type_class_add_private (object_class, sizeof (GtkCellAreaBoxContextPrivate));
}

/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_box_context_finalize (GObject *object)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (object);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;

  g_array_free (priv->base_widths, TRUE);
  g_array_free (priv->base_heights, TRUE);

  g_free (priv->orientation_allocs);

  G_OBJECT_CLASS (gtk_cell_area_box_context_parent_class)->finalize (object);
}

/*************************************************************
 *                    GtkCellAreaContextClass                   *
 *************************************************************/
static void
gtk_cell_area_box_context_flush_preferred_width (GtkCellAreaContext *context)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;
  gint                          i;

  for (i = 0; i < priv->base_widths->len; i++)
    {
      BaseSize *size = &g_array_index (priv->base_widths, BaseSize, i);

      size->min_size = 0;
      size->nat_size = 0;
    }

  GTK_CELL_AREA_CONTEXT_CLASS
    (gtk_cell_area_box_context_parent_class)->flush_preferred_width (context);
}

static void
gtk_cell_area_box_context_flush_preferred_height (GtkCellAreaContext *context)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;
  gint                          i;

  for (i = 0; i < priv->base_heights->len; i++)
    {
      BaseSize *size = &g_array_index (priv->base_heights, BaseSize, i);

      size->min_size = 0;
      size->nat_size = 0;
    }

  GTK_CELL_AREA_CONTEXT_CLASS
    (gtk_cell_area_box_context_parent_class)->flush_preferred_height (context);
}

static void
gtk_cell_area_box_context_flush_allocation (GtkCellAreaContext *context)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;

  g_free (priv->orientation_allocs);
  priv->orientation_allocs   = NULL;
  priv->n_orientation_allocs = 0;
}

static void
gtk_cell_area_box_context_sum_preferred_width (GtkCellAreaContext *context)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;
  GtkCellArea                  *area;
  GtkOrientation                orientation;
  gint                          spacing, i;
  gint                          min_size = 0, nat_size = 0;

  area        = gtk_cell_area_context_get_area (context);
  spacing     = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));

  for (i = 0; i < priv->base_widths->len; i++)
    {
      BaseSize *size = &g_array_index (priv->base_widths, BaseSize, i);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  /* Dont add spacing for 0 size groups, they can be 0 size because
	   * they contain only invisible cells for this round of requests 
	   */
	  if (min_size > 0 && size->nat_size > 0)
	    {
	      min_size += spacing;
	      nat_size += spacing;
	    }
	  
	  min_size += size->min_size;
	  nat_size += size->nat_size;
	}
      else
	{
	  min_size = MAX (min_size, size->min_size);
	  nat_size = MAX (nat_size, size->nat_size);
	}
    }

  gtk_cell_area_context_push_preferred_width (context, min_size, nat_size);
}

static void
gtk_cell_area_box_context_sum_preferred_height (GtkCellAreaContext *context)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GtkCellAreaBoxContextPrivate *priv     = box_context->priv;
  GtkCellArea                  *area;
  GtkOrientation                orientation;
  gint                          spacing, i;
  gint                          min_size = 0, nat_size = 0;

  area        = gtk_cell_area_context_get_area (context);
  spacing     = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));

  for (i = 0; i < priv->base_heights->len; i++)
    {
      BaseSize *size = &g_array_index (priv->base_heights, BaseSize, i);

      if (orientation == GTK_ORIENTATION_VERTICAL)
	{
	  /* Dont add spacing for 0 size groups, they can be 0 size because
	   * they contain only invisible cells for this round of requests 
	   */
	  if (min_size > 0 && size->nat_size > 0)
	    {
	      min_size += spacing;
	      nat_size += spacing;
	    }
	  
	  min_size += size->min_size;
	  nat_size += size->nat_size;
	}
      else
	{
	  min_size = MAX (min_size, size->min_size);
	  nat_size = MAX (nat_size, size->nat_size);
	}
    }

  gtk_cell_area_context_push_preferred_height (context, min_size, nat_size);
}

static GtkRequestedSize *
gtk_cell_area_box_context_get_requests (GtkCellAreaBoxContext *box_context,
					GtkOrientation         orientation,
					gint                  *n_requests)
{
  GtkCellAreaBoxContextPrivate *priv;
  GtkRequestedSize             *requests;
  GArray                       *base_array;
  BaseSize                     *size;
  gint                          visible_groups = 0;
  gint                          i, j;

  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context), NULL);

  priv = box_context->priv;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    base_array = priv->base_widths;
  else
    base_array = priv->base_heights;

  for (i = 0; i < base_array->len; i++)
    {
      size = &g_array_index (base_array, BaseSize, i);

      if (size->nat_size > 0)
	visible_groups++;
    }

  requests = g_new (GtkRequestedSize, visible_groups);

  for (j = 0, i = 0; i < base_array->len; i++)
    {
      size = &g_array_index (base_array, BaseSize, i);

      if (size->nat_size > 0)
	{
	  requests[j].data         = GINT_TO_POINTER (i);
	  requests[j].minimum_size = size->min_size;
	  requests[j].natural_size = size->nat_size;
	  j++;
	}
    }

  if (n_requests)
    *n_requests = visible_groups;

  return requests;
}

static GtkCellAreaBoxAllocation *
allocate_for_orientation (GtkCellAreaBoxContext *context,
			  GtkOrientation         orientation,
			  gint                   spacing,
			  gint                   size,
			  gint                  *n_allocs)
{
  GtkCellAreaBoxContextPrivate *priv = context->priv;
  GtkRequestedSize             *orientation_sizes;
  GtkCellAreaBoxAllocation     *allocs;
  gint                          n_expand_groups = 0;
  gint                          i, n_groups, position;
  gint                          extra_size, extra_extra;
  gint                          avail_size = size;

  orientation_sizes =
    gtk_cell_area_box_context_get_requests (context, orientation, &n_groups);

  /* Count groups that expand */
  for (i = 0; i < n_groups; i++)
    {
      BaseSize *size;
      gint      group_idx = GPOINTER_TO_INT (orientation_sizes[i].data);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	size = &g_array_index (priv->base_widths, BaseSize, group_idx);
      else
	size = &g_array_index (priv->base_heights, BaseSize, group_idx);

      if (size->expand)
	n_expand_groups++;
    }

  /* First start by naturally allocating space among groups */
  avail_size -= (n_groups - 1) * spacing;
  for (i = 0; i < n_groups; i++)
    avail_size -= orientation_sizes[i].minimum_size;

  avail_size = gtk_distribute_natural_allocation (avail_size, n_groups, orientation_sizes);

  /* Calculate/distribute expand for groups */
  if (n_expand_groups > 0)
    {
      extra_size  = avail_size / n_expand_groups;
      extra_extra = avail_size % n_expand_groups;
    }
  else
    extra_size = extra_extra = 0;

  allocs = g_new (GtkCellAreaBoxAllocation, n_groups);

  for (position = 0, i = 0; i < n_groups; i++)
    {
      BaseSize *base_size = &g_array_index (priv->base_widths, BaseSize, i);

      allocs[i].group_idx = GPOINTER_TO_INT (orientation_sizes[i].data);
      allocs[i].position  = position;
      allocs[i].size      = orientation_sizes[i].minimum_size;

      if (base_size->expand)
	{
	  allocs[i].size += extra_size;
	  if (extra_extra)
	    {
	      allocs[i].size++;
	      extra_extra--;
	    }
	}

      position += allocs[i].size;
      position += spacing;
    }

  if (n_allocs)
    *n_allocs = n_groups;

  g_free (orientation_sizes);

  return allocs;
}

static void
gtk_cell_area_box_context_allocate_width (GtkCellAreaContext *context,
					  gint                width)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GtkCellAreaBoxContextPrivate *priv        = box_context->priv;
  GtkCellArea                  *area;
  GtkOrientation                orientation;

  area        = gtk_cell_area_context_get_area (context);
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gint spacing = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));

      g_free (priv->orientation_allocs);
      priv->orientation_allocs = allocate_for_orientation (box_context, orientation, spacing, width,
							   &priv->n_orientation_allocs);
    }

  GTK_CELL_AREA_CONTEXT_CLASS (gtk_cell_area_box_context_parent_class)->allocate_width (context, width);
}

static void
gtk_cell_area_box_context_allocate_height (GtkCellAreaContext *context,
					   gint                height)
{
  GtkCellAreaBoxContext        *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GtkCellAreaBoxContextPrivate *priv     = box_context->priv;
  GtkCellArea                  *area;
  GtkOrientation                orientation;

  area        = gtk_cell_area_context_get_area (context);
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint spacing = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));

      g_free (priv->orientation_allocs);
      priv->orientation_allocs = allocate_for_orientation (box_context, orientation, spacing, height,
							   &priv->n_orientation_allocs);
    }

  GTK_CELL_AREA_CONTEXT_CLASS (gtk_cell_area_box_context_parent_class)->allocate_height (context, height);
}


/*************************************************************
 *                            API                            *
 *************************************************************/
void
gtk_cell_area_box_init_groups (GtkCellAreaBoxContext *box_context,
			       guint                  n_groups,
			       gboolean              *expand_groups)
{
  GtkCellAreaBoxContextPrivate *priv;
  gint                          i;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));
  g_return_if_fail (n_groups == 0 || expand_groups != NULL);

  /* When the group dimensions change, all info must be flushed 
   * Note this already clears the min/nat values on the BaseSizes
   */
  gtk_cell_area_context_flush (GTK_CELL_AREA_CONTEXT (box_context));

  priv = box_context->priv;
  g_array_set_size (priv->base_widths,  n_groups);
  g_array_set_size (priv->base_heights, n_groups);

  /* Now set the expand info */
  for (i = 0; i < n_groups; i++)
    {
      BaseSize *base_width  = &g_array_index (priv->base_widths,  BaseSize, i);
      BaseSize *base_height = &g_array_index (priv->base_heights, BaseSize, i);

      base_width->expand  = expand_groups[i];
      base_height->expand = expand_groups[i];
    }
}

void
gtk_cell_area_box_context_push_group_width (GtkCellAreaBoxContext *box_context,
					    gint                   group_idx,
					    gint                   minimum_width,
					    gint                   natural_width)
{
  GtkCellAreaBoxContextPrivate *priv;
  BaseSize                     *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  size = &g_array_index (priv->base_widths, BaseSize, group_idx);
  size->min_size = MAX (size->min_size, minimum_width);
  size->nat_size = MAX (size->nat_size, natural_width);
}

void
gtk_cell_area_box_context_push_group_height (GtkCellAreaBoxContext *box_context,
					     gint                   group_idx,
					     gint                   minimum_height,
					     gint                   natural_height)
{
  GtkCellAreaBoxContextPrivate *priv;
  BaseSize                     *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_heights->len);

  size = &g_array_index (priv->base_heights, BaseSize, group_idx);
  size->min_size = MAX (size->min_size, minimum_height);
  size->nat_size = MAX (size->nat_size, natural_height);
}

void
gtk_cell_area_box_context_get_group_width (GtkCellAreaBoxContext *box_context,
					   gint                   group_idx,
					   gint                  *minimum_width,
					   gint                  *natural_width)
{
  GtkCellAreaBoxContextPrivate *priv;
  BaseSize                     *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  size = &g_array_index (priv->base_widths, BaseSize, group_idx);

  if (minimum_width)
    *minimum_width = size->min_size;
  
  if (natural_width)
    *natural_width = size->nat_size;
}

void
gtk_cell_area_box_context_get_group_height (GtkCellAreaBoxContext *box_context,
					    gint                   group_idx,
					    gint                  *minimum_height,
					    gint                  *natural_height)
{
  GtkCellAreaBoxContextPrivate *priv;
  BaseSize                     *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (box_context));

  priv = box_context->priv;
  g_return_if_fail (group_idx < priv->base_heights->len);

  size = &g_array_index (priv->base_heights, BaseSize, group_idx);

  if (minimum_height)
    *minimum_height = size->min_size;
  
  if (natural_height)
    *natural_height = size->nat_size;
}

GtkRequestedSize *
gtk_cell_area_box_context_get_widths (GtkCellAreaBoxContext *box_context,
				      gint                  *n_widths)
{
  return gtk_cell_area_box_context_get_requests (box_context, GTK_ORIENTATION_HORIZONTAL, n_widths);
}

GtkRequestedSize *
gtk_cell_area_box_context_get_heights (GtkCellAreaBoxContext *box_context,
				       gint                  *n_heights)
{
  return gtk_cell_area_box_context_get_requests (box_context, GTK_ORIENTATION_VERTICAL, n_heights);
}

GtkCellAreaBoxAllocation *
gtk_cell_area_box_context_get_orientation_allocs (GtkCellAreaBoxContext *context,
						  gint                  *n_allocs)
{
  GtkCellAreaBoxContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (context), NULL);
  
  priv = context->priv;

  *n_allocs = priv->n_orientation_allocs;

  return priv->orientation_allocs;

}

GtkCellAreaBoxAllocation *
gtk_cell_area_box_context_allocate (GtkCellAreaBoxContext *context,
				    gint                   orientation_size,
				    gint                  *n_allocs)
{
  GtkCellArea    *area;
  GtkOrientation  orientation;
  gint            spacing;

  area        = gtk_cell_area_context_get_area (GTK_CELL_AREA_CONTEXT (context));
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));
  spacing     = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));

  return allocate_for_orientation (context, orientation, spacing, orientation_size, n_allocs);
}
