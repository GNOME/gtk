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

/* GObjectClass */
static void      gtk_cell_area_box_iter_finalize                         (GObject            *object);

/* GtkCellAreaIterClass */
static void      gtk_cell_area_box_iter_sum_preferred_width              (GtkCellAreaIter *iter);
static void      gtk_cell_area_box_iter_sum_preferred_height_for_width   (GtkCellAreaIter *iter,
									  gint             width);
static void      gtk_cell_area_box_iter_sum_preferred_height             (GtkCellAreaIter *iter);
static void      gtk_cell_area_box_iter_sum_preferred_width_for_height   (GtkCellAreaIter *iter,
									  gint             height);
static void      gtk_cell_area_box_iter_flush_preferred_width            (GtkCellAreaIter *iter);
static void      gtk_cell_area_box_iter_flush_preferred_height_for_width (GtkCellAreaIter *iter,
									  gint             width);
static void      gtk_cell_area_box_iter_flush_preferred_height           (GtkCellAreaIter *iter);
static void      gtk_cell_area_box_iter_flush_preferred_width_for_height (GtkCellAreaIter *iter,
									  gint             height);

/* CachedSize management */
typedef struct {
  gint min_size;
  gint nat_size;
} CachedSize;

static CachedSize *cached_size_new  (gint min_size, gint nat_size);
static void        cached_size_free (CachedSize *size);

struct _GtkCellAreaBoxIterPrivate
{
  /* Table of per renderer CachedSizes */
  GHashTable *base_widths;
  GHashTable *base_heights;

  /* Table of per height/width hash tables of per renderer CachedSizes */
  GHashTable *widths;
  GHashTable *heights;
};

G_DEFINE_TYPE (GtkCellAreaBoxIter, gtk_cell_area_box_iter, GTK_TYPE_CELL_AREA_ITER);

static void
gtk_cell_area_box_iter_init (GtkCellAreaBoxIter *box_iter)
{
  GtkCellAreaBoxIterPrivate *priv;

  box_iter->priv = G_TYPE_INSTANCE_GET_PRIVATE (box_iter,
						GTK_TYPE_CELL_AREA_BOX_ITER,
						GtkCellAreaBoxIterPrivate);
  priv = box_iter->priv;

  priv->base_widths  = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					      NULL, (GDestroyNotify)cached_size_free);
  priv->base_heights = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					      NULL, (GDestroyNotify)cached_size_free);

  priv->widths       = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					      NULL, (GDestroyNotify)g_hash_table_destroy);
  priv->heights      = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					      NULL, (GDestroyNotify)g_hash_table_destroy);
}

static void 
gtk_cell_area_box_iter_class_init (GtkCellAreaBoxIterClass *class)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (class);
  GtkCellAreaIterClass *iter_class   = GTK_CELL_AREA_ITER_CLASS (class);

  /* GObjectClass */
  object_class->finalize = gtk_cell_area_box_iter_finalize;

  iter_class->sum_preferred_width            = gtk_cell_area_box_iter_sum_preferred_width;
  iter_class->sum_preferred_height_for_width = gtk_cell_area_box_iter_sum_preferred_height_for_width;
  iter_class->sum_preferred_height           = gtk_cell_area_box_iter_sum_preferred_height;
  iter_class->sum_preferred_width_for_height = gtk_cell_area_box_iter_sum_preferred_width_for_height;

  iter_class->flush_preferred_width            = gtk_cell_area_box_iter_flush_preferred_width;
  iter_class->flush_preferred_height_for_width = gtk_cell_area_box_iter_flush_preferred_height_for_width;
  iter_class->flush_preferred_height           = gtk_cell_area_box_iter_flush_preferred_height;
  iter_class->flush_preferred_width_for_height = gtk_cell_area_box_iter_flush_preferred_width_for_height;

  g_type_class_add_private (object_class, sizeof (GtkCellAreaBoxIterPrivate));
}

/*************************************************************
 *                      Cached Sizes                         *
 *************************************************************/
static CachedSize *
cached_size_new (gint min_size, 
		 gint nat_size)
{
  CachedSize *size = g_slice_new (CachedSize);

  size->min_size = min_size;
  size->nat_size = nat_size;

  return size;
}

static void
cached_size_free (CachedSize *size)
{
  g_slice_free (CachedSize, size);
}

/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_box_iter_finalize (GObject *object)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (object);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;

  g_hash_table_destroy (priv->base_widths);
  g_hash_table_destroy (priv->base_heights);
  g_hash_table_destroy (priv->widths);
  g_hash_table_destroy (priv->heights);

  G_OBJECT_CLASS (gtk_cell_area_box_iter_parent_class)->finalize (object);
}

/*************************************************************
 *                    GtkCellAreaIterClass                   *
 *************************************************************/
typedef struct {
  gint min_size;
  gint nat_size;
  gint spacing;
} AccumData;

static void
sum_base_size (gpointer    group_id,
	       CachedSize *size,
	       AccumData  *accum)
{
  if (accum->min_size > 0)
    {
      accum->min_size += accum->spacing;
      accum->nat_size += accum->spacing;
    }

  accum->min_size += size->min_size;
  accum->nat_size += size->nat_size;
}

static void
sum_for_size (gpointer    group_id,
	      CachedSize *size,
	      AccumData  *accum)
{
  accum->min_size = MAX (accum->min_size, size->min_size);
  accum->nat_size = MAX (accum->nat_size, size->nat_size);
}

static void
gtk_cell_area_box_iter_sum_preferred_width (GtkCellAreaIter *iter)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  GtkCellArea               *area;
  AccumData                  accum    = { 0, };

  area          = gtk_cell_area_iter_get_area (iter);
  accum.spacing = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));

  g_hash_table_foreach (priv->base_widths, (GHFunc)sum_base_size, &accum);
  gtk_cell_area_iter_push_preferred_width (iter, accum.min_size, accum.nat_size);
}

static void
gtk_cell_area_box_iter_sum_preferred_height_for_width (GtkCellAreaIter *iter,
						       gint             width)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  GHashTable                *group_table;
  AccumData                  accum    = { 0, };

  group_table = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (width));

  if (group_table)
    {
      g_hash_table_foreach (priv->base_widths, (GHFunc)sum_for_size, &accum);
      gtk_cell_area_iter_push_preferred_height_for_width (iter, width, accum.min_size, accum.nat_size);
    }
}

static void
gtk_cell_area_box_iter_sum_preferred_height (GtkCellAreaIter *iter)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  GtkCellArea               *area;
  AccumData                  accum    = { 0, };

  area          = gtk_cell_area_iter_get_area (iter);
  accum.spacing = gtk_cell_area_box_get_spacing (GTK_CELL_AREA_BOX (area));

  g_hash_table_foreach (priv->base_heights, (GHFunc)sum_base_size, &accum);
  gtk_cell_area_iter_push_preferred_width (iter, accum.min_size, accum.nat_size);
}

static void
gtk_cell_area_box_iter_sum_preferred_width_for_height (GtkCellAreaIter *iter,
						       gint             height)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  GHashTable                *group_table;
  AccumData                  accum    = { 0, };

  group_table = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (height));

  if (group_table)
    {
      g_hash_table_foreach (priv->base_widths, (GHFunc)sum_for_size, &accum);
      gtk_cell_area_iter_push_preferred_width_for_height (iter, height, accum.min_size, accum.nat_size);
    }
}

static void
gtk_cell_area_box_iter_flush_preferred_width (GtkCellAreaIter *iter)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  
  g_hash_table_remove_all (priv->base_widths);

  GTK_CELL_AREA_ITER_GET_CLASS
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

  GTK_CELL_AREA_ITER_GET_CLASS
    (gtk_cell_area_box_iter_parent_class)->flush_preferred_height_for_width (iter, width);
}

static void
gtk_cell_area_box_iter_flush_preferred_height (GtkCellAreaIter *iter)
{
  GtkCellAreaBoxIter        *box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  GtkCellAreaBoxIterPrivate *priv     = box_iter->priv;
  
  g_hash_table_remove_all (priv->base_heights);

  GTK_CELL_AREA_ITER_GET_CLASS
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

  GTK_CELL_AREA_ITER_GET_CLASS
    (gtk_cell_area_box_iter_parent_class)->flush_preferred_width_for_height (iter, height);
}

/*************************************************************
 *                            API                            *
 *************************************************************/

void
gtk_cell_area_box_iter_push_group_width (GtkCellAreaBoxIter *box_iter,
					 gint                group_id,
					 gint                minimum_width,
					 gint                natural_width)
{
  GtkCellAreaBoxIterPrivate *priv;
  CachedSize                *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv = box_iter->priv;
  size = g_hash_table_lookup (priv->base_widths, GINT_TO_POINTER (group_id));

  if (!size)
    {
      size = cached_size_new (minimum_width, natural_width);
      g_hash_table_insert (priv->base_widths, GINT_TO_POINTER (group_id), size);
    }
  else
    {
      size->min_size = MAX (size->min_size, minimum_width);
      size->nat_size = MAX (size->nat_size, natural_width);
    }
}

void
gtk_cell_area_box_iter_push_group_height_for_width  (GtkCellAreaBoxIter *box_iter,
						     gint                group_id,
						     gint                for_width,
						     gint                minimum_height,
						     gint                natural_height)
{
  GtkCellAreaBoxIterPrivate *priv;
  GHashTable                *group_table;
  CachedSize                *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv        = box_iter->priv;
  group_table = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_width));

  if (!group_table)
    {
      group_table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					  NULL, (GDestroyNotify)cached_size_free);

      g_hash_table_insert (priv->heights, GINT_TO_POINTER (for_width), group_table);
    }

  size = g_hash_table_lookup (group_table, GINT_TO_POINTER (group_id));

  if (!size)
    {
      size = cached_size_new (minimum_height, natural_height);
      g_hash_table_insert (group_table, GINT_TO_POINTER (group_id), size);
    }
  else
    {
      size->min_size = MAX (size->min_size, minimum_height);
      size->nat_size = MAX (size->nat_size, natural_height);
    }
}

void
gtk_cell_area_box_iter_push_group_height (GtkCellAreaBoxIter *box_iter,
					  gint                group_id,
					  gint                minimum_height,
					  gint                natural_height)
{
  GtkCellAreaBoxIterPrivate *priv;
  CachedSize                *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv = box_iter->priv;
  size = g_hash_table_lookup (priv->base_heights, GINT_TO_POINTER (group_id));

  if (!size)
    {
      size = cached_size_new (minimum_height, natural_height);
      g_hash_table_insert (priv->base_widths, GINT_TO_POINTER (group_id), size);
    }
  else
    {
      size->min_size = MAX (size->min_size, minimum_height);
      size->nat_size = MAX (size->nat_size, natural_height);
    }
}

void
gtk_cell_area_box_iter_push_group_width_for_height (GtkCellAreaBoxIter *box_iter,
						    gint                group_id,
						    gint                for_height,
						    gint                minimum_width,
						    gint                natural_width)
{
  GtkCellAreaBoxIterPrivate *priv;
  GHashTable                *group_table;
  CachedSize                *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv        = box_iter->priv;
  group_table = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_height));

  if (!group_table)
    {
      group_table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					  NULL, (GDestroyNotify)cached_size_free);

      g_hash_table_insert (priv->widths, GINT_TO_POINTER (for_height), group_table);
    }

  size = g_hash_table_lookup (group_table, GINT_TO_POINTER (group_id));

  if (!size)
    {
      size = cached_size_new (minimum_width, natural_width);
      g_hash_table_insert (group_table, GINT_TO_POINTER (group_id), size);
    }
  else
    {
      size->min_size = MAX (size->min_size, minimum_width);
      size->nat_size = MAX (size->nat_size, natural_width);
    }
}

void
gtk_cell_area_box_iter_get_group_width (GtkCellAreaBoxIter *box_iter,
					gint                group_id,
					gint               *minimum_width,
					gint               *natural_width)
{
  GtkCellAreaBoxIterPrivate *priv;
  CachedSize                *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv = box_iter->priv;
  size = g_hash_table_lookup (priv->base_widths, GINT_TO_POINTER (group_id));

  if (size)
    {
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

void
gtk_cell_area_box_iter_get_group_height_for_width (GtkCellAreaBoxIter *box_iter,
						   gint                group_id,
						   gint                for_width,
						   gint               *minimum_height,
						   gint               *natural_height)
{
  GtkCellAreaBoxIterPrivate *priv;
  GHashTable                *group_table;
  CachedSize                *size = NULL;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv        = box_iter->priv;
  group_table = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_width));

  if (group_table)
    size = g_hash_table_lookup (group_table, GINT_TO_POINTER (group_id));

  if (size)
    {
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
					 gint                group_id,
					 gint               *minimum_height,
					 gint               *natural_height)
{
  GtkCellAreaBoxIterPrivate *priv;
  CachedSize                *size;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv = box_iter->priv;
  size = g_hash_table_lookup (priv->base_heights, GINT_TO_POINTER (group_id));

  if (size)
    {
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
gtk_cell_area_box_iter_get_group_width_for_height (GtkCellAreaBoxIter *box_iter,
						   gint                group_id,
						   gint                for_height,
						   gint               *minimum_width,
						   gint               *natural_width)
{
  GtkCellAreaBoxIterPrivate *priv;
  GHashTable                *group_table;
  CachedSize                *size = NULL;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter));

  priv        = box_iter->priv;
  group_table = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_height));

  if (group_table)
    size = g_hash_table_lookup (group_table, GINT_TO_POINTER (group_id));

  if (size)
    {
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

static void
fill_requested_sizes (gpointer          group_id_ptr,
		      CachedSize       *cached_size,
		      GtkRequestedSize *requested)
{
  gint group_id = GPOINTER_TO_INT (group_id_ptr);

  requested[group_id].data         = group_id_ptr;
  requested[group_id].minimum_size = cached_size->min_size;
  requested[group_id].natural_size = cached_size->nat_size;
}

GtkRequestedSize *
gtk_cell_area_box_iter_get_widths (GtkCellAreaBoxIter *box_iter,
				   gint               *n_widths)
{
  GtkCellAreaBoxIterPrivate *priv;
  GtkRequestedSize          *widths;

  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter), NULL);

  priv = box_iter->priv;

  *n_widths = g_hash_table_size (priv->widths);
  widths    = g_new (GtkRequestedSize, *n_widths);

  g_hash_table_foreach (priv->widths, (GHFunc)fill_requested_sizes, widths);

  return widths;
}

GtkRequestedSize *
gtk_cell_area_box_iter_get_heights (GtkCellAreaBoxIter *box_iter,
				    gint               *n_heights)
{
  GtkCellAreaBoxIterPrivate *priv;
  GtkRequestedSize          *heights;

  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX_ITER (box_iter), NULL);

  priv = box_iter->priv;

  *n_heights = g_hash_table_size (priv->heights);
  heights    = g_new (GtkRequestedSize, *n_heights);

  g_hash_table_foreach (priv->heights, (GHFunc)fill_requested_sizes, heights);

  return heights;
}
