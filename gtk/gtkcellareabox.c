/* gtkcellareabox.c
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
#include "gtkorientable.h"
#include "gtkcelllayout.h"
#include "gtkcellareabox.h"
#include "gtkcellareaboxiter.h"
#include "gtkprivate.h"


/* GObjectClass */
static void      gtk_cell_area_box_finalize                       (GObject            *object);
static void      gtk_cell_area_box_dispose                        (GObject            *object);
static void      gtk_cell_area_box_set_property                   (GObject            *object,
								   guint               prop_id,
								   const GValue       *value,
								   GParamSpec         *pspec);
static void      gtk_cell_area_box_get_property                   (GObject            *object,
								   guint               prop_id,
								   GValue             *value,
								   GParamSpec         *pspec);

/* GtkCellAreaClass */
static void      gtk_cell_area_box_add                            (GtkCellArea        *area,
								   GtkCellRenderer    *renderer);
static void      gtk_cell_area_box_remove                         (GtkCellArea        *area,
								   GtkCellRenderer    *renderer);
static void      gtk_cell_area_box_forall                         (GtkCellArea        *area,
								   GtkCellCallback     callback,
								   gpointer            callback_data);
static gint      gtk_cell_area_box_event                          (GtkCellArea        *area,
								   GtkWidget          *widget,
								   GdkEvent           *event,
								   const GdkRectangle *cell_area);
static void      gtk_cell_area_box_render                         (GtkCellArea        *area,
								   cairo_t            *cr,
								   GtkWidget          *widget,
								   const GdkRectangle *cell_area);

static GtkCellAreaIter    *gtk_cell_area_box_create_iter          (GtkCellArea        *area);
static GtkSizeRequestMode  gtk_cell_area_box_get_request_mode     (GtkCellArea        *area);
static void      gtk_cell_area_box_get_preferred_width            (GtkCellArea        *area,
								   GtkCellAreaIter    *iter,
								   GtkWidget          *widget,
								   gint               *minimum_width,
								   gint               *natural_width);
static void      gtk_cell_area_box_get_preferred_height           (GtkCellArea        *area,
								   GtkCellAreaIter    *iter,
								   GtkWidget          *widget,
								   gint               *minimum_height,
								   gint               *natural_height);
static void      gtk_cell_area_box_get_preferred_height_for_width (GtkCellArea        *area,
								   GtkCellAreaIter    *iter,
								   GtkWidget          *widget,
								   gint                width,
								   gint               *minimum_height,
								   gint               *natural_height);
static void      gtk_cell_area_box_get_preferred_width_for_height (GtkCellArea        *area,
								   GtkCellAreaIter    *iter,
								   GtkWidget          *widget,
								   gint                height,
								   gint               *minimum_width,
								   gint               *natural_width);

/* GtkCellLayoutIface */
static void      gtk_cell_area_box_cell_layout_init               (GtkCellLayoutIface *iface);
static void      gtk_cell_area_box_layout_pack_start              (GtkCellLayout      *cell_layout,
								   GtkCellRenderer    *renderer,
								   gboolean            expand);
static void      gtk_cell_area_box_layout_pack_end                (GtkCellLayout      *cell_layout,
								   GtkCellRenderer    *renderer,
								   gboolean            expand);
static void      gtk_cell_area_box_layout_reorder                 (GtkCellLayout      *cell_layout,
								   GtkCellRenderer    *renderer,
								   gint                position);


/* CellInfo metadata handling */
typedef struct {
  GtkCellRenderer *renderer;

  guint            expand : 1;
  guint            pack   : 1;
} CellInfo;

static CellInfo  *cell_info_new  (GtkCellRenderer *renderer, 
				  gboolean         expand,
				  GtkPackType      pack);
static void       cell_info_free (CellInfo        *info);
static gint       cell_info_find (CellInfo        *info,
				  GtkCellRenderer *renderer);


struct _GtkCellAreaBoxPrivate
{
  GtkOrientation orientation;

  GList         *cells;

  gint           spacing;

  guint          align_cells : 1;
};

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_SPACING,
  PROP_ALIGN_CELLS
};

G_DEFINE_TYPE_WITH_CODE (GtkCellAreaBox, gtk_cell_area_box, GTK_TYPE_CELL_AREA,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
						gtk_cell_area_box_cell_layout_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL));

static void
gtk_cell_area_box_init (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv;

  box->priv = G_TYPE_INSTANCE_GET_PRIVATE (box,
                                           GTK_TYPE_CELL_AREA_BOX,
                                           GtkCellAreaBoxPrivate);
  priv = box->priv;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->cells       = NULL;
  priv->spacing     = 0;
  priv->align_cells = TRUE;
}

static void 
gtk_cell_area_box_class_init (GtkCellAreaBoxClass *class)
{
  GObjectClass     *object_class = G_OBJECT_CLASS (class);
  GtkCellAreaClass *area_class   = GTK_CELL_AREA_CLASS (class);

  /* GObjectClass */
  object_class->finalize     = gtk_cell_area_box_finalize;
  object_class->dispose      = gtk_cell_area_box_dispose;
  object_class->set_property = gtk_cell_area_box_set_property;
  object_class->get_property = gtk_cell_area_box_get_property;

  /* GtkCellAreaClass */
  area_class->add                            = gtk_cell_area_box_add;
  area_class->remove                         = gtk_cell_area_box_remove;
  area_class->forall                         = gtk_cell_area_box_forall;
  area_class->event                          = gtk_cell_area_box_event;
  area_class->render                         = gtk_cell_area_box_render;
  
  area_class->create_iter                    = gtk_cell_area_box_create_iter;
  area_class->get_request_mode               = gtk_cell_area_box_get_request_mode;
  area_class->get_preferred_width            = gtk_cell_area_box_get_preferred_width;
  area_class->get_preferred_height           = gtk_cell_area_box_get_preferred_height;
  area_class->get_preferred_height_for_width = gtk_cell_area_box_get_preferred_height_for_width;
  area_class->get_preferred_width_for_height = gtk_cell_area_box_get_preferred_width_for_height;

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  g_object_class_install_property (object_class,
                                   PROP_SPACING,
                                   g_param_spec_int ("spacing",
						     P_("Spacing"),
						     P_("Space which is inserted between cells"),
						     0,
						     G_MAXINT,
						     0,
						     GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ALIGN_CELLS,
                                   g_param_spec_boolean ("align-cells",
							 P_("Align Cells"),
							 P_("Whether cells should be aligned with those "
							    "rendered in adjacent rows"),
							 TRUE,
							 GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkCellAreaBoxPrivate));
}


/*************************************************************
 *                    CellInfo Basics                        *
 *************************************************************/
static CellInfo *
cell_info_new  (GtkCellRenderer *renderer, 
		gboolean         expand,
		GtkPackType      pack)
{
  CellInfo *info = g_slice_new (CellInfo);
  
  info->renderer = g_object_ref_sink (renderer);
  info->expand   = expand;
  info->pack     = pack;

  return info;
}

static void
cell_info_free (CellInfo *info)
{
  g_object_unref (info->renderer);

  g_slice_free (CellInfo, info);
}

static gint
cell_info_find (CellInfo        *info,
		GtkCellRenderer *renderer)
{
  return (info->renderer == renderer) ? 0 : -1;
}

/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_box_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_cell_area_box_parent_class)->finalize (object);
}

static void
gtk_cell_area_box_dispose (GObject *object)
{
  G_OBJECT_CLASS (gtk_cell_area_box_parent_class)->dispose (object);
}

static void
gtk_cell_area_box_set_property (GObject       *object,
				guint          prop_id,
				const GValue  *value,
				GParamSpec    *pspec)
{

}

static void
gtk_cell_area_box_get_property (GObject     *object,
				guint        prop_id,
				GValue      *value,
				GParamSpec  *pspec)
{

}

/*************************************************************
 *                    GtkCellAreaClass                       *
 *************************************************************/
static void      
gtk_cell_area_box_add (GtkCellArea        *area,
		       GtkCellRenderer    *renderer)
{
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area),
				renderer, FALSE);
}

static void
gtk_cell_area_box_remove (GtkCellArea        *area,
			  GtkCellRenderer    *renderer)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = box->priv;
  GList                 *node;

  node = g_list_find_custom (priv->cells, renderer, 
			     (GCompareFunc)cell_info_find);

  if (node)
    {
      CellInfo *info = node->data;

      cell_info_free (info);

      priv->cells = g_list_delete_link (priv->cells, node);
    }
  else
    g_warning ("Trying to remove a cell renderer that is not present GtkCellAreaBox");
}

static void
gtk_cell_area_box_forall (GtkCellArea        *area,
			  GtkCellCallback     callback,
			  gpointer            callback_data)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = box->priv;
  GList                 *list;

  for (list = priv->cells; list; list = list->next)
    {
      CellInfo *info = list->data;

      callback (info->renderer, callback_data);
    }
}

static gint
gtk_cell_area_box_event (GtkCellArea        *area,
			 GtkWidget          *widget,
			 GdkEvent           *event,
			 const GdkRectangle *cell_area)
{


  return 0;
}

static void
gtk_cell_area_box_render (GtkCellArea        *area,
			  cairo_t            *cr,
			  GtkWidget          *widget,
			  const GdkRectangle *cell_area)
{

}

static GtkCellAreaIter *
gtk_cell_area_box_create_iter (GtkCellArea *area)
{
  return (GtkCellAreaIter *)g_object_new (GTK_TYPE_CELL_AREA_BOX_ITER, NULL);
}

static GtkSizeRequestMode 
gtk_cell_area_box_get_request_mode (GtkCellArea *area)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = box->priv;

  return (priv->orientation) == GTK_ORIENTATION_HORIZONTAL ?
    GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH :
    GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void
get_renderer_size (GtkCellRenderer    *renderer,
		   GtkOrientation      orientation,
		   GtkWidget          *widget,
		   gint                for_size,
		   gint               *minimum_size,
		   gint               *natural_size)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (for_size < 0)
	gtk_cell_renderer_get_preferred_width (renderer, widget, minimum_size, natural_size);
      else
	gtk_cell_renderer_get_preferred_width_for_height (renderer, widget, for_size, 
							  minimum_size, natural_size);
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      if (for_size < 0)
	gtk_cell_renderer_get_preferred_height (renderer, widget, minimum_size, natural_size);
      else
	gtk_cell_renderer_get_preferred_height_for_width (renderer, widget, for_size, 
							  minimum_size, natural_size);
    }
}

static void
compute_size (GtkCellAreaBox     *box,
	      GtkOrientation      orientation,
	      GtkCellAreaBoxIter *iter,
	      GtkWidget          *widget,
	      gint               *minimum_size,
	      gint               *natural_size)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  CellInfo              *info;
  GList                 *l;
  gint                   min_size = 0;
  gint                   nat_size = 0;
  gboolean               first_cell = TRUE;
  
  for (l = priv->cells; l; l = l->next)
    {
      gint renderer_min_size, renderer_nat_size;

      info = l->data;

      get_renderer_size (info->renderer, orientation, widget, -1, 
			 &renderer_min_size, &renderer_nat_size);

      /* If we're aligning the cells we need to cache the max results
       * for all requests performed with the same iter.
       */
      if (priv->align_cells)
	{
	  if (orientation == GTK_ORIENTATION_HORIZONTAL)
	    gtk_cell_area_box_iter_push_cell_width (iter, info->renderer, 
						    renderer_min_size, renderer_nat_size);
	  else
	    gtk_cell_area_box_iter_push_cell_height (iter, info->renderer, 
						     renderer_min_size, renderer_nat_size);
	}

      if (orientation == priv->orientation)
	{
	  min_size += renderer_min_size;
	  nat_size += renderer_nat_size;

	  if (!first_cell)
	    {
	      min_size += priv->spacing;
	      nat_size += priv->spacing;
	    }
	}
      else
	{
	  min_size = MAX (min_size, renderer_min_size);
	  nat_size = MAX (nat_size, renderer_nat_size);
	}

      if (first_cell)
	first_cell = FALSE;
    }

  *minimum_size = min_size;
  *natural_size = nat_size;
}

static void
update_iter_aligned (GtkCellAreaBox     *box,
		     GtkCellAreaBoxIter *iter,
		     gint                for_size)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  CellInfo              *info;
  GList                 *l;
  gint                   min_size = 0;
  gint                   nat_size = 0;
  gboolean               first_cell = TRUE;

  for (l = priv->cells; l; l = l->next)
    {
      gint aligned_min_size, aligned_nat_size;
      
      info = l->data;

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  if (for_size < 0)
	    gtk_cell_area_box_iter_get_cell_width (iter, info->renderer, 
						   &aligned_min_size,
						   &aligned_nat_size);
	  else
	    gtk_cell_area_box_iter_get_cell_width_for_height (iter, info->renderer, 
							      for_size,
							      &aligned_min_size,
							      &aligned_nat_size);
	}
      else
	{
	  if (for_size < 0)
	    gtk_cell_area_box_iter_get_cell_height (iter, info->renderer, 
						    &aligned_min_size,
						    &aligned_nat_size);
	  else
	    gtk_cell_area_box_iter_get_cell_height_for_width (iter, info->renderer, 
							      for_size,
							      &aligned_min_size,
							      &aligned_nat_size);
	}

      min_size += aligned_min_size;
      nat_size += aligned_nat_size;
      
      if (!first_cell)
	{
	  min_size += priv->spacing;
	  nat_size += priv->spacing;
	}
      
      if (first_cell)
	first_cell = FALSE;
    }

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (for_size < 0)
	gtk_cell_area_iter_push_preferred_width (GTK_CELL_AREA_ITER (iter), min_size, nat_size);
      else
	gtk_cell_area_iter_push_preferred_width_for_height (GTK_CELL_AREA_ITER (iter), 
							    for_size, min_size, nat_size);
    }
  else
    {
      if (for_size < 0)
	gtk_cell_area_iter_push_preferred_height (GTK_CELL_AREA_ITER (iter), min_size, nat_size);
      else
	gtk_cell_area_iter_push_preferred_height_for_width (GTK_CELL_AREA_ITER (iter), 
							    for_size, min_size, nat_size);
    }
}

static void
gtk_cell_area_box_get_preferred_width (GtkCellArea        *area,
				       GtkCellAreaIter    *iter,
				       GtkWidget          *widget,
				       gint               *minimum_width,
				       gint               *natural_width)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxIter    *box_iter;
  GtkCellAreaBoxPrivate *priv;
  gint                   min_width, nat_width;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (iter));

  box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  priv     = box->priv;

  /* Compute the size of all renderers for current row data, possibly
   * bumping cell alignments in the iter along the way */
  compute_size (box, GTK_ORIENTATION_HORIZONTAL,
		box_iter, widget, &min_width, &nat_width);

  /* Update width of the iter based on aligned cell sizes if 
   * appropriate */
  if (priv->align_cells && priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    update_iter_aligned (box, box_iter, -1);
  else
    gtk_cell_area_iter_push_preferred_width (iter, min_width, nat_width);

  if (minimum_width)
    *minimum_width = min_width;

  if (natural_width)
    *natural_width = nat_width;
}

static void
gtk_cell_area_box_get_preferred_height (GtkCellArea        *area,
					GtkCellAreaIter    *iter,
					GtkWidget          *widget,
					gint               *minimum_height,
					gint               *natural_height)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxIter    *box_iter;
  GtkCellAreaBoxPrivate *priv;
  gint                   min_height, nat_height;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (iter));

  box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  priv     = box->priv;

  /* Compute the size of all renderers for current row data, possibly
   * bumping cell alignments in the iter along the way */
  compute_size (box, GTK_ORIENTATION_VERTICAL,
		box_iter, widget, &min_height, &nat_height);

  /* Update width of the iter based on aligned cell sizes if 
   * appropriate */
  if (priv->align_cells && priv->orientation == GTK_ORIENTATION_VERTICAL)
    update_iter_aligned (box, box_iter, -1);
  else
    gtk_cell_area_iter_push_preferred_height (iter, min_height, nat_height);

  if (minimum_height)
    *minimum_height = min_height;

  if (natural_height)
    *natural_height = nat_height;
}

static void
compute_size_for_orientation (GtkCellAreaBox     *box, 
			      GtkCellAreaBoxIter *iter, 
			      GtkWidget          *widget, 
			      gint                for_size,
			      gint               *minimum_size, 
			      gint               *natural_size)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  CellInfo              *info;
  GList                 *l;
  gint                   min_size = 0;
  gint                   nat_size = 0;
  gboolean               first_cell = TRUE;
  
  for (l = priv->cells; l; l = l->next)
    {
      gint renderer_min_size, renderer_nat_size;

      info = l->data;

      get_renderer_size (info->renderer, priv->orientation, widget, for_size, 
			 &renderer_min_size, &renderer_nat_size);

      /* If we're aligning the cells we need to cache the max results
       * for all requests performed with the same iter.
       */
      if (priv->align_cells)
	{
	  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	    gtk_cell_area_box_iter_push_cell_width_for_height (iter, info->renderer, for_size,
							       renderer_min_size, renderer_nat_size);
	  else
	    gtk_cell_area_box_iter_push_cell_height_for_width (iter, info->renderer, for_size,
							       renderer_min_size, renderer_nat_size);
	}

      min_size += renderer_min_size;
      nat_size += renderer_nat_size;
      
      if (!first_cell)
	{
	  min_size += priv->spacing;
	  nat_size += priv->spacing;
	}
      
      if (first_cell)
	first_cell = FALSE;
    }

  *minimum_size = min_size;
  *natural_size = nat_size;
}


static void
gtk_cell_area_box_get_preferred_height_for_width (GtkCellArea        *area,
						  GtkCellAreaIter    *iter,
						  GtkWidget          *widget,
						  gint                width,
						  gint               *minimum_height,
						  gint               *natural_height)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxIter    *box_iter;
  GtkCellAreaBoxPrivate *priv;
  gint                   min_height, nat_height;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (iter));

  box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  priv     = box->priv;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      /* Add up vertical requests of height for width and possibly push the overall 
       * cached sizes for alignments */
      compute_size_for_orientation (box, box_iter, widget, width, &min_height, &nat_height);

      /* Update the overall cached height for width based on aligned cells if appropriate */
      if (priv->align_cells)
	update_iter_aligned (box, box_iter, width);
      else
	gtk_cell_area_iter_push_preferred_height_for_width (GTK_CELL_AREA_ITER (iter),
							    width, min_height, nat_height);
    }
  else
    {
      /* XXX Juice: virtually allocate cells into the for_width possibly using the 
       * alignments and then return the overall height for that width, and cache it */
    }

  if (minimum_height)
    *minimum_height = min_height;

  if (natural_height)
    *natural_height = nat_height;
}

static void
gtk_cell_area_box_get_preferred_width_for_height (GtkCellArea        *area,
						  GtkCellAreaIter    *iter,
						  GtkWidget          *widget,
						  gint                height,
						  gint               *minimum_width,
						  gint               *natural_width)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxIter    *box_iter;
  GtkCellAreaBoxPrivate *priv;
  gint                   min_width, nat_width;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (iter));

  box_iter = GTK_CELL_AREA_BOX_ITER (iter);
  priv     = box->priv;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* Add up vertical requests of height for width and possibly push the overall 
       * cached sizes for alignments */
      compute_size_for_orientation (box, box_iter, widget, height, &min_width, &nat_width);

      /* Update the overall cached height for width based on aligned cells if appropriate */
      if (priv->align_cells)
	update_iter_aligned (box, box_iter, height);
      else
	gtk_cell_area_iter_push_preferred_height_for_width (GTK_CELL_AREA_ITER (iter),
							    height, min_width, nat_width);
    }
  else
    {
      /* XXX Juice: virtually allocate cells into the for_width possibly using the 
       * alignments and then return the overall height for that width, and cache it */
    }

  if (minimum_width)
    *minimum_width = min_width;

  if (natural_width)
    *natural_width = nat_width;
}


/*************************************************************
 *                    GtkCellLayoutIface                     *
 *************************************************************/
static void
gtk_cell_area_box_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start = gtk_cell_area_box_layout_pack_start;
  iface->pack_end   = gtk_cell_area_box_layout_pack_end;
  iface->reorder    = gtk_cell_area_box_layout_reorder;
}

static void
gtk_cell_area_box_layout_pack_start (GtkCellLayout      *cell_layout,
				     GtkCellRenderer    *renderer,
				     gboolean            expand)
{
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (cell_layout), renderer, expand);
}

static void
gtk_cell_area_box_layout_pack_end (GtkCellLayout      *cell_layout,
				   GtkCellRenderer    *renderer,
				   gboolean            expand)
{
  gtk_cell_area_box_pack_end (GTK_CELL_AREA_BOX (cell_layout), renderer, expand);
}

static void
gtk_cell_area_box_layout_reorder (GtkCellLayout      *cell_layout,
				  GtkCellRenderer    *renderer,
				  gint                position)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (cell_layout);
  GtkCellAreaBoxPrivate *priv = box->priv;
  GList                 *node;
  CellInfo              *info;
  
  node = g_list_find_custom (priv->cells, renderer, 
			     (GCompareFunc)cell_info_find);

  if (node)
    {
      info = node->data;

      priv->cells = g_list_delete_link (priv->cells, node);
      priv->cells = g_list_insert (priv->cells, info, position);
    }
}

/*************************************************************
 *                            API                            *
 *************************************************************/
GtkCellArea *
gtk_cell_area_box_new (void)
{
  return (GtkCellArea *)g_object_new (GTK_TYPE_CELL_AREA_BOX, NULL);
}

void
gtk_cell_area_box_pack_start  (GtkCellAreaBox  *box,
			       GtkCellRenderer *renderer,
			       gboolean         expand)
{
  GtkCellAreaBoxPrivate *priv;
  CellInfo              *info;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX (box));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  priv = box->priv;

  if (g_list_find_custom (priv->cells, renderer, 
			  (GCompareFunc)cell_info_find))
    {
      g_warning ("Refusing to add the same cell renderer to a GtkCellAreaBox twice");
      return;
    }

  info = cell_info_new (renderer, expand, GTK_PACK_START);

  priv->cells = g_list_append (priv->cells, info);
}

void
gtk_cell_area_box_pack_end (GtkCellAreaBox  *box,
			    GtkCellRenderer *renderer,
			    gboolean         expand)
{
  GtkCellAreaBoxPrivate *priv;
  CellInfo              *info;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX (box));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  priv = box->priv;

  if (g_list_find_custom (priv->cells, renderer, 
			  (GCompareFunc)cell_info_find))
    {
      g_warning ("Refusing to add the same cell renderer to a GtkCellArea twice");
      return;
    }

  info = cell_info_new (renderer, expand, GTK_PACK_END);

  priv->cells = g_list_append (priv->cells, info);
}

gint
gtk_cell_area_box_get_spacing (GtkCellAreaBox  *box)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX (box), 0);

  return box->priv->spacing;
}

void
gtk_cell_area_box_set_spacing (GtkCellAreaBox  *box,
			       gint             spacing)
{
  GtkCellAreaBoxPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX (box));

  priv = box->priv;

  if (priv->spacing != spacing)
    {
      priv->spacing = spacing;

      g_object_notify (G_OBJECT (box), "spacing");
    }
}

gboolean
gtk_cell_area_box_get_align_cells (GtkCellAreaBox  *box)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA_BOX (box), FALSE);

  return box->priv->align_cells;
}

void
gtk_cell_area_box_set_align_cells (GtkCellAreaBox  *box,
				   gboolean         align)
{
  GtkCellAreaBoxPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX (box));

  priv = box->priv;

  if (priv->align_cells != align)
    {
      priv->align_cells = align;

      g_object_notify (G_OBJECT (box), "align-cells");
    }
}
