/* gtkcellareaboxiter.c
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
#include "gtkcellareaboxiter.h"
#include "gtkorientable.h"

/* GObjectClass */
static void      gtk_cell_area_box_iter_finalize                         (GObject            *object);

/* GtkCellAreaIterClass */
static void      gtk_cell_area_box_iter_flush_preferred_width            (GtkCellAreaIter *iter);
static void      gtk_cell_area_box_iter_flush_preferred_height_for_width (GtkCellAreaIter *iter,
									  gint             width);
static void      gtk_cell_area_box_iter_flush_preferred_height           (GtkCellAreaIter *iter);
static void      gtk_cell_area_box_iter_flush_preferred_width_for_height (GtkCellAreaIter *iter,
									  gint             height);
static void      gtk_cell_area_box_iter_flush_allocation                 (GtkCellAreaIter *iter);
static void      gtk_cell_area_box_iter_sum_preferred_width              (GtkCellAreaIter *iter);
static void      gtk_cell_area_box_iter_sum_preferred_height_for_width   (GtkCellAreaIter *iter,
									  gint             width);
static void      gtk_cell_area_box_iter_sum_preferred_height             (GtkCellAreaIter *iter);
static void      gtk_cell_area_box_iter_sum_preferred_width_for_height   (GtkCellAreaIter *iter,
									  gint             height);
static void      gtk_cell_area_box_iter_allocate_width                   (GtkCellAreaIter *iter,
									  gint             width);
static void      gtk_cell_area_box_iter_allocate_height                  (GtkCellAreaIter *iter,
									  gint             height);

static void      free_cache_array                                        (GArray          *array);

/* CachedSize management */
typedef struct {
  gint     min_size;
  gint     nat_size;
} CachedSize;

typedef struct {
  gint     min_size;
  gint     nat_size;
  gboolean expand;
} BaseSize;

struct _GtkCellAreaBoxIterPrivate
{
  /* Table of per renderer CachedSizes */
  GArray *base_widths;
  GArray *base_heights;

  /* Table of per height/width hash tables of per renderer CachedSizes */
  GHashTable *widths;
  GHashTable *heights;

  /* Allocation info for this iter if any */
  gint                      alloc_width;
  gint                      alloc_height;
  gint                      n_orientation_allocs;
  GtkCellAreaBoxAllocation *orientation_allocs;
};

G_DEFINE_TYPE (GtkCellAreaBoxIter, gtk_cell_area_box_iter, GTK_TYPE_CELL_AREA_ITER);

static void
free_cache_array (GArray *array)
{
  g_array_free (array, TRUE);
}

static void
gtk_cell_area_box_iter_init (GtkCellAreaBoxIter *box_iter)
{
  GtkCellAreaBoxIterPrivate *priv;

  box_iter->priv = G_TYPE_INSTANCE_GET_PRIVATE (box_iter,
						GTK_TYPE_CELL_AREA_BOX_ITER,
						GtkCellAreaBoxIterPrivate);
  priv = box_iter->priv;

  priv->base_widths  = g_array_new (FALSE, TRUE, sizeof (BaseSize));
  priv->base_heights = g_array_new (FALSE, TRUE, sizeof (BaseSize));

  priv->widths       = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					      NULL, (GDestroyNotify)free_cache_array);
  priv->heights      = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					      NULL, (GDestroyNotify)free_cache_array);

  priv->alloc_width  = 0;
  priv->alloc_height = 0;
  priv->orientation_allocs   = NULL;
  priv->n_orientation_allocs = 0;
}

static void 
gtk_cell_area_box_iter_class_init (GtkCellAreaBoxIterClass *class)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (class);
  GtkCellAreaIterClass *iter_class   = GTK_CELL_AREA_ITER_CLASS (class);

  /* GObjectClass */
  object_class->finalize = gtk_cell_area_box_iter_finalize;

  iter_class->flush_preferred_width            = gtk_cell_area_box_iter_flush_preferred_width;
  iter_class->flush_preferred_height_for_width = gtk_cell_area_box_iter_flush_preferred_height_for_width;
  iter_class->flush_preferred_height           = gtk_cell_area_box_iter_flush_preferred_height;
  iter_class->flush_preferred_width_for_height = gtk_cell_area_box_iter_flush_preferred_width_for_height;
  iter_class->flush_allocation                 = gtk_cell_area_box_iter_flush_allocation;

  iter_class->sum_preferred_width            = gtk_cell_area_box_iter_sum_preferred_width;
  iter_class->sum_preferred_height_for_width = gtk_cell_area_box_iter_sum_preferred_height_for_width;
  iter_class->sum_preferred_height           = gtk_cell_area_box_iter_sum_preferred_height;
  iter_class->sum_preferred_width_for_height = gtk_cell_area_box_iter_sum_preferred_width_for_height;

  iter_class->allocate_width  = gtk_cell_area_box_iter_allocate_width;
  iter_class->allocate_height = gtk_cell_area_box_iter_allocate_height;

  g_type_class_add_private (object_class, sizeof (GtkCellAreaBoxIterPrivate));
}

/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_box_iter_finalize (GObject *object)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (object);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;

  g_array_free (priv->base_widths, TRUE);
  g_array_free (priv->base_heights, TRUE);
  g_hash_table_destroy (priv->widths);
  g_hash_table_destroy (priv->heights);

  g_free (priv->orientation_allocs);

  G_OBJECT_CLASS (gtk_cell_area_box_iter_parent_class)->finalize (object);
}

/*************************************************************
 *                    GtkCellAreaIterClass                   *
 *************************************************************/
static void
gtk_cell_area_box_iter_flush_preferred_width (GtkCellAreaIter *iter)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  gint                       i;

  for (i = 0; i < priv->base_widths->len; i++)
    {
      BaseSize *size = &g_array_index (priv->base_widths, BaseSize, i);

      size->min_size = 0;
      size->nat_size = 0;
    }

  GTK_CELL_AREA_ITER_CLASS
    (gtk_cell_area_box_iter_parent_class)->flush_preferred_width (iter);
}

static void
gtk_cell_area_box_iter_flush_preferred_height_for_width (GtkCellAreaIter *iter,
							 gint             width)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;

  /* Flush all sizes for special -1 value */
  if (width < 0)
    g_hash_table_remove_all (priv->heights);
  else
    g_hash_table_remove (priv->heights, GINT_TO_POINTER (width));

  GTK_CELL_AREA_ITER_CLASS
    (gtk_cell_area_box_iter_parent_class)->flush_preferred_height_for_width (iter, width);
}

static void
gtk_cell_area_box_iter_flush_preferred_height (GtkCellAreaIter *iter)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  gint                       i;

  for (i = 0; i < priv->base_heights->len; i++)
    {
      BaseSize *size = &g_array_index (priv->base_heights, BaseSize, i);

      size->min_size = 0;
      size->nat_size = 0;
    }

  GTK_CELL_AREA_ITER_CLASS
    (gtk_cell_area_box_iter_parent_class)->flush_preferred_height (iter);
}

static void
gtk_cell_area_box_iter_flush_preferred_width_for_height (GtkCellAreaIter *iter,
							 gint             height)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;

  /* Flush all sizes for special -1 value */
  if (height < 0)
    g_hash_table_remove_all (priv->widths);
  else
    g_hash_table_remove (priv->widths, GINT_TO_POINTER (height));

  GTK_CELL_AREA_ITER_CLASS
    (gtk_cell_area_box_iter_parent_class)->flush_preferred_width_for_height (iter, height);
}

static void
gtk_cell_area_box_iter_flush_allocation (GtkCellAreaIter *iter)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;

  g_free (priv->orientation_allocs);
  priv->orientation_allocs   = NULL;
  priv->n_orientation_allocs = 0;
}

static void
gtk_cell_area_box_iter_sum_preferred_width (GtkCellAreaIter *iter)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  GtkCellArea               *area;
  GtkOrientation             orientation;
  gint                       spacing, i;
  gint                       min_size = 0, nat_size = 0;

  area        = gtk_cell_area_iter_get_area (iter);
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

  gtk_cell_area_iter_push_preferred_width (iter, min_size, nat_size);
}

static void
gtk_cell_area_box_iter_sum_preferred_height_for_width (GtkCellAreaIter *iter,
						       gint             width)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  GArray                    *group_array;
  GtkCellArea               *area;
  GtkOrientation             orientation;
  gint                       spacing, i;
  gint                       min_size = 0, nat_size = 0;

  group_array = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (width));

  if (group_array)
    {
      area        = gtk_cell_area_iter_get_area (iter);
      spacing     = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));
      orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));
      
      for (i = 0; i < group_array->len; i++)
	{
	  CachedSize *size = &g_array_index (group_array, CachedSize, i);
	  
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

      gtk_cell_area_iter_push_preferred_height_for_width (iter, width, min_size, nat_size);
    }
}

static void
gtk_cell_area_box_iter_sum_preferred_height (GtkCellAreaIter *iter)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  GtkCellArea               *area;
  GtkOrientation             orientation;
  gint                       spacing, i;
  gint                       min_size = 0, nat_size = 0;

  area        = gtk_cell_area_iter_get_area (iter);
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

  gtk_cell_area_iter_push_preferred_height (iter, min_size, nat_size);
}

static void
gtk_cell_area_box_iter_sum_preferred_width_for_height (GtkCellAreaIter *iter,
						       gint             height)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  GArray                    *group_array;
  GtkCellArea               *area;
  GtkOrientation             orientation;
  gint                       spacing, i;
  gint                       min_size = 0, nat_size = 0;

  group_array = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (height));

  if (group_array)
    {
      area        = gtk_cell_area_iter_get_area (iter);
      spacing     = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));
      orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));
      
      for (i = 0; i < group_array->len; i++)
	{
	  CachedSize *size = &g_array_index (group_array, CachedSize, i);
	  
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

      gtk_cell_area_iter_push_preferred_width_for_height (iter, height, min_size, nat_size);
    }
}

static GtkRequestedSize *
gtk_cell_area_box_iter_get_requests (GtkCellAreaBoxIter *box_iter,
				     GtkOrientation      orientation,
				     gint               *n_requests)
{
  GtkCellAreaBoxIterPrivate *priv;
  GtkRequestedSize          *requests;
  GArray                    *base_array;
  BaseSize                  *size;
  gint                       visible_groups = 0;
  gint                       i, j;

  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter), NULL);

  priv = box_iter->priv;

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
allocate_for_orientation (GtkCellAreaBoxIter *iter,
			  GtkOrientation      orientation,
			  gint                spacing,
			  gint                size,
			  gint               *n_allocs)
{
  GtkCellAreaBoxIterPrivate *priv = iter->priv;
  GtkRequestedSize          *orientation_sizes;
  GtkCellAreaBoxAllocation  *allocs;
  gint                       n_expand_groups = 0;
  gint                       i, n_groups, position;
  gint                       extra_size, extra_extra;
  gint                       avail_size = size;

  orientation_sizes =
    gtk_cell_area_box_iter_get_requests (iter, 
					 GTK_ORIENTATION_HORIZONTAL, 
					 &n_groups);

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
gtk_cell_area_box_iter_allocate_width (GtkCellAreaIter *iter,
				       gint             width)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  GtkCellArea               *area;
  GtkOrientation             orientation;

  area        = gtk_cell_area_iter_get_area (iter);
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gint spacing = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));

      g_free (priv->orientation_allocs);
      priv->orientation_allocs = allocate_for_orientation (box_iter, orientation, spacing, width,
							   &priv->n_orientation_allocs);
    }

  GTK_CELL_AREA_ITER_CLASS (gtk_cell_area_box_iter_parent_class)->allocate_width (iter, width);
}

static void
gtk_cell_area_box_iter_allocate_height (GtkCellAreaIter *iter,
					gint             height)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  GtkCellArea               *area;
  GtkOrientation             orientation;

  area        = gtk_cell_area_iter_get_area (iter);
  orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (area));

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      gint spacing = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));

      g_free (priv->orientation_allocs);
      priv->orientation_allocs = allocate_for_orientation (box_iter, orientation, spacing, height,
							   &priv->n_orientation_allocs);
    }

  GTK_CELL_AREA_ITER_CLASS (gtk_cell_area_box_iter_parent_class)->allocate_height (iter, height);
}

/*************************************************************
 *                            API                            *
 *************************************************************/
void
gtk_cell_area_box_init_groups (GtkCellAreaBoxIter *box_iter,
			       guint               n_groups,
			       gboolean           *expand_groups)
{
  GtkCellAreaBoxIterPrivate *priv;
  gint                       i;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));
  g_return_if_fail (n_groups == 0 || expand_groups != NULL);

  /* When the group dimensions change, all info must be flushed 
   * Note this already clears the min/nat values on the BaseSizes
   */
  gtk_cell_area_iter_flush (GTK_CELL_AREA_ITER (box_iter));

  priv = box_iter->priv;
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
gtk_cell_area_box_iter_push_group_width (GtkCellAreaBoxIter *box_iter,
					 gint                group_idx,
					 gint                minimum_width,
					 gint                natural_width)
{
  GtkCellAreaBoxIterPrivate *priv;
  BaseSize                  *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv = box_iter->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  size = &g_array_index (priv->base_widths, BaseSize, group_idx);
  size->min_size = MAX (size->min_size, minimum_width);
  size->nat_size = MAX (size->nat_size, natural_width);
}

void
gtk_cell_area_box_iter_push_group_height_for_width  (GtkCellAreaBoxIter *box_iter,
						     gint                group_idx,
						     gint                for_width,
						     gint                minimum_height,
						     gint                natural_height)
{
  GtkCellAreaBoxIterPrivate *priv;
  GArray                    *group_array;
  CachedSize                *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv        = box_iter->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_width));
  if (!group_array)
    {
      group_array = g_array_new (FALSE, TRUE, sizeof (CachedSize));
      g_array_set_size (group_array, priv->base_heights->len);

      g_hash_table_insert (priv->heights, GINT_TO_POINTER (for_width), group_array);
    }

  size = &g_array_index (group_array, CachedSize, group_idx);
  size->min_size = MAX (size->min_size, minimum_height);
  size->nat_size = MAX (size->nat_size, natural_height);
}

void
gtk_cell_area_box_iter_push_group_height (GtkCellAreaBoxIter *box_iter,
					  gint                group_idx,
					  gint                minimum_height,
					  gint                natural_height)
{
  GtkCellAreaBoxIterPrivate *priv;
  BaseSize                  *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv = box_iter->priv;
  g_return_if_fail (group_idx < priv->base_heights->len);

  size = &g_array_index (priv->base_heights, BaseSize, group_idx);
  size->min_size = MAX (size->min_size, minimum_height);
  size->nat_size = MAX (size->nat_size, natural_height);
}

void
gtk_cell_area_box_iter_push_group_width_for_height (GtkCellAreaBoxIter *box_iter,
						    gint                group_idx,
						    gint                for_height,
						    gint                minimum_width,
						    gint                natural_width)
{
  GtkCellAreaBoxIterPrivate *priv;
  GArray                    *group_array;
  CachedSize                *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv        = box_iter->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_height));
  if (!group_array)
    {
      group_array = g_array_new (FALSE, TRUE, sizeof (CachedSize));
      g_array_set_size (group_array, priv->base_heights->len);

      g_hash_table_insert (priv->widths, GINT_TO_POINTER (for_height), group_array);
    }

  size = &g_array_index (group_array, CachedSize, group_idx);
  size->min_size = MAX (size->min_size, minimum_width);
  size->nat_size = MAX (size->nat_size, natural_width);
}

void
gtk_cell_area_box_iter_get_group_width (GtkCellAreaBoxIter *box_iter,
					gint                group_idx,
					gint               *minimum_width,
					gint               *natural_width)
{
  GtkCellAreaBoxIterPrivate *priv;
  BaseSize                  *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv = box_iter->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  size = &g_array_index (priv->base_widths, BaseSize, group_idx);

  if (minimum_width)
    *minimum_width = size->min_size;
  
  if (natural_width)
    *natural_width = size->nat_size;
}

void
gtk_cell_area_box_iter_get_group_height_for_width (GtkCellAreaBoxIter *box_iter,
						   gint                group_idx,
						   gint                for_width,
						   gint               *minimum_height,
						   gint               *natural_height)
{
  GtkCellAreaBoxIterPrivate *priv;
  GArray                    *group_array;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv        = box_iter->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_width));

  if (group_array)
    {
      CachedSize *size = &g_array_index (group_array, CachedSize, group_idx);

      if (minimum_height)
	*minimum_height = size->min_size;

      if (natural_height)
	*natural_height = size->nat_size;
    }
  else
    {
      if (minimum_height)
	*minimum_height = -1;

      if (natural_height)
	*natural_height = -1;      
    }
}

void
gtk_cell_area_box_iter_get_group_height (GtkCellAreaBoxIter *box_iter,
					 gint                group_idx,
					 gint               *minimum_height,
					 gint               *natural_height)
{
  GtkCellAreaBoxIterPrivate *priv;
  BaseSize                  *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv = box_iter->priv;
  g_return_if_fail (group_idx < priv->base_heights->len);

  size = &g_array_index (priv->base_heights, BaseSize, group_idx);

  if (minimum_height)
    *minimum_height = size->min_size;
  
  if (natural_height)
    *natural_height = size->nat_size;
}

void
gtk_cell_area_box_iter_get_group_width_for_height (GtkCellAreaBoxIter *box_iter,
						   gint                group_idx,
						   gint                for_height,
						   gint               *minimum_width,
						   gint               *natural_width)
{
  GtkCellAreaBoxIterPrivate *priv;
  GArray                    *group_array;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv        = box_iter->priv;
  g_return_if_fail (group_idx < priv->base_widths->len);

  group_array = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_height));

  if (group_array)
    {
      CachedSize *size = &g_array_index (group_array, CachedSize, group_idx);

      if (minimum_width)
	*minimum_width = size->min_size;

      if (natural_width)
	*natural_width = size->nat_size;
    }
  else
    {
      if (minimum_width)
	*minimum_width = -1;

      if (natural_width)
	*natural_width = -1;      
    }
}

GtkRequestedSize *
gtk_cell_area_box_iter_get_widths (GtkCellAreaBoxIter *box_iter,
				   gint               *n_widths)
{
  return gtk_cell_area_box_iter_get_requests (box_iter, GTK_ORIENTATION_HORIZONTAL, n_widths);
}

GtkRequestedSize *
gtk_cell_area_box_iter_get_heights (GtkCellAreaBoxIter *box_iter,
				    gint               *n_heights)
{
  return gtk_cell_area_box_iter_get_requests (box_iter, GTK_ORIENTATION_VERTICAL, n_heights);
}

G_CONST_RETURN GtkCellAreaBoxAllocation *
gtk_cell_area_box_iter_get_orientation_allocs (GtkCellAreaBoxIter *iter,
					       gint               *n_allocs)
{
  GtkCellAreaBoxIterPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX_ITER (iter), NULL);
  
  priv = iter->priv;

  *n_allocs = priv->n_orientation_allocs;

  return priv->orientation_allocs;
}
