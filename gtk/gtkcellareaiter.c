/* gtkcellareaiter.c
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
#include "gtkmarshalers.h"
#include "gtkcellareaiter.h"
#include "gtkprivate.h"

/* GObjectClass */
static void      gtk_cell_area_iter_finalize                       (GObject            *object);
static void      gtk_cell_area_iter_dispose                        (GObject            *object);
static void      gtk_cell_area_iter_get_property                   (GObject           *object,
								   guint               prop_id,
								   GValue             *value,
								   GParamSpec         *pspec);
static void      gtk_cell_area_iter_set_property                  (GObject            *object,
								   guint               prop_id,
								   const GValue       *value,
								   GParamSpec         *pspec);

/* GtkCellAreaIterClass */
static void      gtk_cell_area_iter_real_flush_preferred_width            (GtkCellAreaIter *iter);
static void      gtk_cell_area_iter_real_flush_preferred_height_for_width (GtkCellAreaIter *iter,
									   gint             width);
static void      gtk_cell_area_iter_real_flush_preferred_height           (GtkCellAreaIter *iter);
static void      gtk_cell_area_iter_real_flush_preferred_width_for_height (GtkCellAreaIter *iter,
									   gint             height);

/* CachedSize management */
typedef struct {
  gint min_size;
  gint nat_size;
} CachedSize;

static CachedSize *cached_size_new  (gint min_size, gint nat_size);
static void        cached_size_free (CachedSize *size);

struct _GtkCellAreaIterPrivate
{
  GtkCellArea *cell_area;

  gint         min_width;
  gint         nat_width;
  gint         min_height;
  gint         nat_height;

  GHashTable  *widths;
  GHashTable  *heights;
};

enum {
  PROP_0,
  PROP_CELL_AREA,
  PROP_MIN_WIDTH,
  PROP_NAT_WIDTH,
  PROP_MIN_HEIGHT,
  PROP_NAT_HEIGHT
};

enum {
  SIGNAL_WIDTH_CHANGED,
  SIGNAL_HEIGHT_CHANGED,
  LAST_SIGNAL
};

static guint cell_area_iter_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkCellAreaIter, gtk_cell_area_iter, G_TYPE_OBJECT);

static void
gtk_cell_area_iter_init (GtkCellAreaIter *iter)
{
  GtkCellAreaIterPrivate *priv;

  iter->priv = G_TYPE_INSTANCE_GET_PRIVATE (iter,
					    GTK_TYPE_CELL_AREA_ITER,
					    GtkCellAreaIterPrivate);
  priv = iter->priv;

  priv->min_width  = -1;
  priv->nat_width  = -1;
  priv->min_height = -1;
  priv->nat_height = -1;
  priv->widths     = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					    NULL, (GDestroyNotify)cached_size_free);
  priv->heights    = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					    NULL, (GDestroyNotify)cached_size_free);
}

static void 
gtk_cell_area_iter_class_init (GtkCellAreaIterClass *class)
{
  GObjectClass     *object_class = G_OBJECT_CLASS (class);

  /* GObjectClass */
  object_class->finalize     = gtk_cell_area_iter_finalize;
  object_class->dispose      = gtk_cell_area_iter_dispose;
  object_class->get_property = gtk_cell_area_iter_get_property;
  object_class->set_property = gtk_cell_area_iter_set_property;

  class->flush_preferred_width            = gtk_cell_area_iter_real_flush_preferred_width;
  class->flush_preferred_height_for_width = gtk_cell_area_iter_real_flush_preferred_height_for_width;
  class->flush_preferred_height           = gtk_cell_area_iter_real_flush_preferred_height;
  class->flush_preferred_width_for_height = gtk_cell_area_iter_real_flush_preferred_width_for_height;

  cell_area_iter_signals[SIGNAL_HEIGHT_CHANGED] =
    g_signal_new (I_("height-changed"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, /* Class offset (just a notification, no class handler) */
		  NULL, NULL,
		  _gtk_marshal_VOID__INT_INT_INT,
		  G_TYPE_NONE, 3,
		  G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

  cell_area_iter_signals[SIGNAL_WIDTH_CHANGED] =
    g_signal_new (I_("width-changed"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, /* Class offset (just a notification, no class handler) */
		  NULL, NULL,
		  _gtk_marshal_VOID__INT_INT_INT,
		  G_TYPE_NONE, 3,
		  G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

  g_object_class_install_property (object_class,
                                   PROP_CELL_AREA,
                                   g_param_spec_object ("area",
							P_("Area"),
							P_("The Cell Area this iter was created for"),
							GTK_TYPE_CELL_AREA,
							GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class,
                                   PROP_MIN_WIDTH,
                                   g_param_spec_int ("minimum-width",
						     P_("Minimum Width"),
						     P_("Minimum cached width"),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_NAT_WIDTH,
                                   g_param_spec_int ("natural-width",
						     P_("Minimum Width"),
						     P_("Minimum cached width"),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_MIN_HEIGHT,
                                   g_param_spec_int ("minimum-height",
						     P_("Minimum Height"),
						     P_("Minimum cached height"),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_NAT_HEIGHT,
                                   g_param_spec_int ("natural-height",
						     P_("Minimum Height"),
						     P_("Minimum cached height"),
						     -1,
						     G_MAXINT,
						     -1,
						     G_PARAM_READABLE));

  g_type_class_add_private (object_class, sizeof (GtkCellAreaIterPrivate));
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
gtk_cell_area_iter_finalize (GObject *object)
{
  GtkCellAreaIter        *iter = GTK_CELL_AREA_ITER (object);
  GtkCellAreaIterPrivate *priv = iter->priv;

  g_hash_table_destroy (priv->widths);
  g_hash_table_destroy (priv->heights);

  G_OBJECT_CLASS (gtk_cell_area_iter_parent_class)->finalize (object);
}

static void
gtk_cell_area_iter_dispose (GObject *object)
{
  GtkCellAreaIter        *iter = GTK_CELL_AREA_ITER (object);
  GtkCellAreaIterPrivate *priv = iter->priv;

  if (priv->cell_area)
    {
      g_object_unref (priv->cell_area);

      priv->cell_area = NULL;
    }

  G_OBJECT_CLASS (gtk_cell_area_iter_parent_class)->dispose (object);
}

static void
gtk_cell_area_iter_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
  GtkCellAreaIter        *iter = GTK_CELL_AREA_ITER (object);
  GtkCellAreaIterPrivate *priv = iter->priv;

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
gtk_cell_area_iter_get_property (GObject     *object,
				 guint        prop_id,
				 GValue      *value,
				 GParamSpec  *pspec)
{
  GtkCellAreaIter        *iter = GTK_CELL_AREA_ITER (object);
  GtkCellAreaIterPrivate *priv = iter->priv;

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
 *                    GtkCellAreaIterClass                   *
 *************************************************************/
static void
gtk_cell_area_iter_real_flush_preferred_width (GtkCellAreaIter *iter)
{
  GtkCellAreaIterPrivate *priv = iter->priv;
  
  priv->min_width = -1;
  priv->nat_width = -1;

  g_object_freeze_notify (G_OBJECT (iter));
  g_object_notify (G_OBJECT (iter), "minimum-width");
  g_object_notify (G_OBJECT (iter), "natural-width");
  g_object_thaw_notify (G_OBJECT (iter));
}

static void
gtk_cell_area_iter_real_flush_preferred_height_for_width (GtkCellAreaIter *iter,
							  gint             width)
{
  GtkCellAreaIterPrivate *priv = iter->priv;

  /* Flush all sizes for special -1 value */
  if (width < 0)
    g_hash_table_remove_all (priv->heights);
  else
    g_hash_table_remove (priv->heights, GINT_TO_POINTER (width));

  /* XXX Should we bother signalling removed values as "size-changed" signals ? */
}

static void
gtk_cell_area_iter_real_flush_preferred_height (GtkCellAreaIter *iter)
{
  GtkCellAreaIterPrivate *priv = iter->priv;
  
  priv->min_height = -1;
  priv->nat_height = -1;

  g_object_freeze_notify (G_OBJECT (iter));
  g_object_notify (G_OBJECT (iter), "minimum-height");
  g_object_notify (G_OBJECT (iter), "natural-height");
  g_object_thaw_notify (G_OBJECT (iter));
}

static void
gtk_cell_area_iter_real_flush_preferred_width_for_height (GtkCellAreaIter *iter,
							  gint             height)
{
  GtkCellAreaIterPrivate *priv = iter->priv;

  /* Flush all sizes for special -1 value */
  if (height < 0)
    g_hash_table_remove_all (priv->widths);
  else
    g_hash_table_remove (priv->widths, GINT_TO_POINTER (height));

  /* XXX Should we bother signalling removed values as "size-changed" signals ? */
}

/*************************************************************
 *                            API                            *
 *************************************************************/
GtkCellArea *
gtk_cell_area_iter_get_area (GtkCellAreaIter *iter)
{
  GtkCellAreaIterPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_AREA_ITER (iter), NULL);

  priv = iter->priv;

  return priv->cell_area;
}

void
gtk_cell_area_iter_get_preferred_width (GtkCellAreaIter *iter,
					gint            *minimum_width,
					gint            *natural_width)
{
  GtkCellAreaIterPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  priv = iter->priv;

  if (minimum_width)
    *minimum_width = priv->min_width;

  if (natural_width)
    *natural_width = priv->nat_width;
}

void
gtk_cell_area_iter_get_preferred_height_for_width (GtkCellAreaIter *iter,
						   gint             for_width,
						   gint            *minimum_height,
						   gint            *natural_height)
{
  GtkCellAreaIterPrivate *priv;
  CachedSize             *size;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  priv = iter->priv;

  size = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_width));

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
gtk_cell_area_iter_get_preferred_height (GtkCellAreaIter *iter,
					 gint            *minimum_height,
					 gint            *natural_height)
{
  GtkCellAreaIterPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  priv = iter->priv;

  if (minimum_height)
    *minimum_height = priv->min_height;

  if (natural_height)
    *natural_height = priv->nat_height;
}

void
gtk_cell_area_iter_get_preferred_width_for_height (GtkCellAreaIter *iter,
						   gint             for_height,
						   gint            *minimum_width,
						   gint            *natural_width)
{
  GtkCellAreaIterPrivate *priv;
  CachedSize             *size;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  priv = iter->priv;

  size = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_height));

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
gtk_cell_area_iter_sum_preferred_width (GtkCellAreaIter *iter)
{
  GtkCellAreaIterClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  class = GTK_CELL_AREA_ITER_GET_CLASS (iter);

  if (class->sum_preferred_width)
    class->sum_preferred_width (iter);
}

void
gtk_cell_area_iter_sum_preferred_height_for_width (GtkCellAreaIter *iter,
						   gint             for_width)
{
  GtkCellAreaIterClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  class = GTK_CELL_AREA_ITER_GET_CLASS (iter);

  if (class->sum_preferred_height_for_width)
    class->sum_preferred_height_for_width (iter, for_width);
}

void
gtk_cell_area_iter_sum_preferred_height (GtkCellAreaIter *iter)
{
  GtkCellAreaIterClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  class = GTK_CELL_AREA_ITER_GET_CLASS (iter);

  if (class->sum_preferred_height)
    class->sum_preferred_height (iter);
}

void
gtk_cell_area_iter_sum_preferred_width_for_height (GtkCellAreaIter *iter,
						   gint             for_height)
{
  GtkCellAreaIterClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  class = GTK_CELL_AREA_ITER_GET_CLASS (iter);

  if (class->sum_preferred_width_for_height)
    class->sum_preferred_width_for_height (iter, for_height);
}

void
gtk_cell_area_iter_flush (GtkCellAreaIter *iter)
{
  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  gtk_cell_area_iter_flush_preferred_width (iter);
  gtk_cell_area_iter_flush_preferred_height_for_width (iter, -1);
  gtk_cell_area_iter_flush_preferred_height (iter);
  gtk_cell_area_iter_flush_preferred_width_for_height (iter, -1);
}

void
gtk_cell_area_iter_flush_preferred_width (GtkCellAreaIter *iter)
{
  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  GTK_CELL_AREA_ITER_GET_CLASS (iter)->flush_preferred_width (iter);
}

void
gtk_cell_area_iter_flush_preferred_height_for_width (GtkCellAreaIter *iter,
						     gint             for_width)
{
  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  GTK_CELL_AREA_ITER_GET_CLASS (iter)->flush_preferred_height_for_width (iter, for_width);
}

void
gtk_cell_area_iter_flush_preferred_height (GtkCellAreaIter *iter)
{
  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  GTK_CELL_AREA_ITER_GET_CLASS (iter)->flush_preferred_height (iter);
}

void
gtk_cell_area_iter_flush_preferred_width_for_height (GtkCellAreaIter *iter,
						     gint             for_height)
{
  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  GTK_CELL_AREA_ITER_GET_CLASS (iter)->flush_preferred_width_for_height (iter, for_height);
}



void
gtk_cell_area_iter_push_preferred_width (GtkCellAreaIter *iter,
					 gint             minimum_width,
					 gint             natural_width)
{
  GtkCellAreaIterPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  priv = iter->priv;

  g_object_freeze_notify (G_OBJECT (iter));

  if (minimum_width > priv->min_width)
    {
      priv->min_width = minimum_width;

      g_object_notify (G_OBJECT (iter), "minimum-width");
    }

  if (natural_width > priv->nat_width)
    {
      priv->nat_width = natural_width;

      g_object_notify (G_OBJECT (iter), "natural-width");
    }

  g_object_thaw_notify (G_OBJECT (iter));
}

void
gtk_cell_area_iter_push_preferred_height_for_width (GtkCellAreaIter *iter,
						    gint             for_width,
						    gint             minimum_height,
						    gint             natural_height)
{
  GtkCellAreaIterPrivate *priv;
  CachedSize             *size;
  gboolean                changed = FALSE;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  priv = iter->priv;

  size = g_hash_table_lookup (priv->heights, GINT_TO_POINTER (for_width));

  if (!size)
    {
      size = cached_size_new (minimum_height, natural_height);

      g_hash_table_insert (priv->heights, GINT_TO_POINTER (for_width), size);

      changed = TRUE;
    }
  else
    {
      if (minimum_height > size->min_size)
	{
	  size->min_size = minimum_height;
	  changed = TRUE;
	}

      if (natural_height > size->nat_size)
	{
	  size->nat_size = natural_height;
	  changed = TRUE;
	}
    }
  
  if (changed)
    g_signal_emit (iter, cell_area_iter_signals[SIGNAL_HEIGHT_CHANGED], 0, 
		   for_width, size->min_size, size->nat_size);
}

void
gtk_cell_area_iter_push_preferred_height (GtkCellAreaIter *iter,
					  gint             minimum_height,
					  gint             natural_height)
{
  GtkCellAreaIterPrivate *priv;
  
  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  priv = iter->priv;

  g_object_freeze_notify (G_OBJECT (iter));

  if (minimum_height > priv->min_height)
    {
      priv->min_height = minimum_height;

      g_object_notify (G_OBJECT (iter), "minimum-height");
    }

  if (natural_height > priv->nat_height)
    {
      priv->nat_height = natural_height;

      g_object_notify (G_OBJECT (iter), "natural-height");
    }

  g_object_thaw_notify (G_OBJECT (iter));
}

void
gtk_cell_area_iter_push_preferred_width_for_height (GtkCellAreaIter *iter,
						    gint             for_height,
						    gint             minimum_width,
						    gint             natural_width)
{
  GtkCellAreaIterPrivate *priv;
  CachedSize             *size;
  gboolean                changed = FALSE;

  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));

  priv = iter->priv;

  size = g_hash_table_lookup (priv->widths, GINT_TO_POINTER (for_height));

  if (!size)
    {
      size = cached_size_new (minimum_width, natural_width);

      g_hash_table_insert (priv->widths, GINT_TO_POINTER (for_height), size);

      changed = TRUE;
    }
  else
    {
      if (minimum_width > size->min_size)
	{
	  size->min_size = minimum_width;
	  changed = TRUE;
	}

      if (natural_width > size->nat_size)
	{
	  size->nat_size = natural_width;
	  changed = TRUE;
	}
    }
  
  if (changed)
    g_signal_emit (iter, cell_area_iter_signals[SIGNAL_WIDTH_CHANGED], 0, 
		   for_height, size->min_size, size->nat_size);
}
