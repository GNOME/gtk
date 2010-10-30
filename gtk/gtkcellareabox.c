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


/* CellInfo/CellGroup metadata handling and convenience functions */
typedef struct {
  GtkCellRenderer *renderer;

  guint            expand : 1; /* Whether the cell expands */
  guint            pack   : 1; /* Whether the cell is packed from the start or end */
  guint            align  : 1; /* Whether to align this cell's position with adjacent rows */
} CellInfo;

typedef struct {
  GList *cells;

  guint  id     : 16;
  guint  expand : 1;
} CellGroup;

static CellInfo  *cell_info_new          (GtkCellRenderer *renderer, 
					  GtkPackType      pack,
					  gboolean         expand,
					  gboolean         align);
static void       cell_info_free         (CellInfo        *info);
static gint       cell_info_find         (CellInfo        *info,
					  GtkCellRenderer *renderer);

static CellGroup *cell_group_new         (guint            id);
static void       cell_group_free        (CellGroup       *group);

static GList     *list_consecutive_cells (GtkCellAreaBox  *box);
static GList     *construct_cell_groups  (GtkCellAreaBox  *box);
static gint       count_expand_groups    (GtkCellAreaBox  *box);
static gint       count_expand_cells     (CellGroup       *group);
static void       iter_weak_notify       (GtkCellAreaBox  *box,
					  GtkCellAreaIter *dead_iter);
static void       flush_iters            (GtkCellAreaBox  *box);

struct _GtkCellAreaBoxPrivate
{
  GtkOrientation  orientation;

  GList          *cells;
  GList          *groups;

  GSList         *iters;

  gint            spacing;
};

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_SPACING
};

G_DEFINE_TYPE_WITH_CODE (GtkCellAreaBox, gtk_cell_area_box, GTK_TYPE_CELL_AREA,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
						gtk_cell_area_box_cell_layout_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL));

#define OPPOSITE_ORIENTATION(orientation)			\
  ((orientation) == GTK_ORIENTATION_HORIZONTAL ?		\
   GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL)

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
  priv->groups      = NULL;
  priv->iters       = NULL;
  priv->spacing     = 0;
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

  g_type_class_add_private (object_class, sizeof (GtkCellAreaBoxPrivate));
}


/*************************************************************
 *    CellInfo/CellGroup basics and convenience functions    *
 *************************************************************/
static CellInfo *
cell_info_new  (GtkCellRenderer *renderer, 
		GtkPackType      pack,
		gboolean         expand,
		gboolean         align)
{
  CellInfo *info = g_slice_new (CellInfo);
  
  info->renderer = g_object_ref_sink (renderer);
  info->pack     = pack;
  info->expand   = expand;
  info->align    = align;

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

static CellGroup *
cell_group_new (guint id)
{
  CellGroup *group = g_slice_new0 (CellGroup);

  group->id = id;

  return group;
}

static void
cell_group_free (CellGroup *group)
{
  g_list_free (group->cells);
  g_slice_free (CellGroup, group);  
}

static GList *
list_consecutive_cells (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  GList                 *l, *consecutive_cells = NULL, *pack_end_cells = NULL;
  CellInfo              *info;

  /* List cells in consecutive order taking their 
   * PACK_START/PACK_END options into account 
   */
  for (l = priv->cells; l; l = l->next)
    {
      info = l->data;
      
      if (info->pack == GTK_PACK_START)
	consecutive_cells = g_list_prepend (consecutive_cells, info);
    }

  for (l = priv->cells; l; l = l->next)
    {
      info = l->data;
      
      if (info->pack == GTK_PACK_END)
	pack_end_cells = g_list_prepend (pack_end_cells, info);
    }

  consecutive_cells = g_list_reverse (consecutive_cells);
  consecutive_cells = g_list_concat (consecutive_cells, pack_end_cells);

  return consecutive_cells;
}

static GList *
construct_cell_groups (GtkCellAreaBox  *box)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  CellGroup             *group;
  GList                 *cells, *l;
  GList                 *groups = NULL;
  guint                  id = 0;

  if (!priv->cells)
    return NULL;

  cells  = list_consecutive_cells (box);
  group  = cell_group_new (id++);
  groups = g_list_prepend (groups, group);

  for (l = cells; l; l = l->next)
    {
      CellInfo *info = l->data;

      /* A new group starts with any aligned cell, the first group is implied */
      if (info->align && l != cells)
	{
	  group  = cell_group_new (id++);
	  groups = g_list_prepend (groups, group);
	}

      group->cells = g_list_prepend (group->cells, info);

      /* A group expands if it contains any expand cells */
      if (info->expand)
	group->expand = TRUE;
    }

  g_list_free (cells);

  for (l = cells; l; l = l->next)
    {
      group = l->data;
      group->cells = g_list_reverse (group->cells);
    }

  return g_list_reverse (groups);
}

static gint
count_expand_groups (GtkCellAreaBox  *box)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  GList                 *l;
  gint                   expand_groups = 0;

  for (l = priv->groups; l; l = l->next)
    {
      CellGroup *group = l->data;

      if (group->expand)
	expand_groups++;
    }

  return expand_groups;
}

static gint
count_expand_cells (CellGroup *group)
{
  GList *l;
  gint   expand_cells = 0;

  if (!group->expand)
    return 0;

  for (l = group->cells; l; l = l->next)
    {
      CellInfo *info = l->data;

      if (info->expand)
	expand_cells++;
    }

  return expand_cells;
}

static void 
iter_weak_notify (GtkCellAreaBox  *box,
		  GtkCellAreaIter *dead_iter)
{
  GtkCellAreaBoxPrivate *priv = box->priv;

  priv->iters = g_slist_remove (priv->iters, dead_iter);
}

static void
flush_iters (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  GSList                *l;

  /* When the box layout changes, iters need to
   * be flushed and sizes for the box get requested again
   */
  for (l = priv->iters; l; l = l->next)
    {
      GtkCellAreaIter *iter = l->data;

      gtk_cell_area_iter_flush (iter);
    }
}


/* XXX This guy makes an allocation to be stored and retrieved from the iter */
GtkCellAreaBoxAllocation *
gtk_cell_area_box_allocate (GtkCellAreaBox     *box,
			    GtkCellAreaBoxIter *iter,
			    gint                size,
			    gint               *n_allocs)
{


}


/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_box_finalize (GObject *object)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (object);
  GtkCellAreaBoxPrivate *priv = box->priv;
  GSList                *l;

  for (l = priv->iters; l; l = l->next)
    g_object_weak_unref (G_OBJECT (l->data), (GWeakNotify)iter_weak_notify, box);

  g_slist_free (priv->iters);

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
  GtkCellAreaBox *box = GTK_CELL_AREA_BOX (object);

  switch (prop_id)
    {
    case PROP_SPACING:
      gtk_cell_area_box_set_spacing (box, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_area_box_get_property (GObject     *object,
				guint        prop_id,
				GValue      *value,
				GParamSpec  *pspec)
{
  GtkCellAreaBox *box = GTK_CELL_AREA_BOX (object);

  switch (prop_id)
    {
    case PROP_SPACING:
      g_value_set_int (value, gtk_cell_area_box_get_spacing (box));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*************************************************************
 *                    GtkCellAreaClass                       *
 *************************************************************/
static void      
gtk_cell_area_box_add (GtkCellArea        *area,
		       GtkCellRenderer    *renderer)
{
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area),
				renderer, FALSE, TRUE);
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

      /* Reconstruct cell groups */
      g_list_foreach (priv->groups, (GFunc)cell_group_free, NULL);
      g_list_free (priv->groups);
      priv->groups = construct_cell_groups (box);

      /* Notify that size needs to be requested again */
      flush_iters (box);
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
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = box->priv;
  GtkCellAreaIter       *iter =
    (GtkCellAreaIter *)g_object_new (GTK_TYPE_CELL_AREA_BOX_ITER, NULL);

  priv->iters = g_slist_prepend (priv->iters, iter);

  g_object_weak_ref (G_OBJECT (iter), (GWeakNotify)iter_weak_notify, box);

  return iter;
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
	      gint                for_size,
	      gint               *minimum_size,
	      gint               *natural_size)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  CellGroup             *group;
  CellInfo              *info;
  GList                 *cell_list, *group_list;
  gint                   min_size = 0;
  gint                   nat_size = 0;
  
  for (group_list = priv->groups; group_list; group_list = group_list->next)
    {
      gint group_min_size = 0;
      gint group_nat_size = 0;

      group = group_list->data;

      for (cell_list = group->cells; cell_list; cell_list = cell_list->next)
	{
	  gint renderer_min_size, renderer_nat_size;
	  
	  info = cell_list->data;
	  
	  get_renderer_size (info->renderer, orientation, widget, for_size, 
			     &renderer_min_size, &renderer_nat_size);

	  if (orientation == priv->orientation)
	    {
	      if (min_size > 0)
		{
		  min_size += priv->spacing;
		  nat_size += priv->spacing;
		}
	      
	      if (group_min_size > 0)
		{
		  group_min_size += priv->spacing;
		  group_nat_size += priv->spacing;
		}
	      
	      min_size       += renderer_min_size;
	      nat_size       += renderer_nat_size;
	      group_min_size += renderer_min_size;
	      group_nat_size += renderer_nat_size;
	    }
	  else
	    {
	      min_size       = MAX (min_size, renderer_min_size);
	      nat_size       = MAX (nat_size, renderer_nat_size);
	      group_min_size = MAX (group_min_size, renderer_min_size);
	      group_nat_size = MAX (group_nat_size, renderer_nat_size);
	    }
	}

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  if (for_size < 0)
	    gtk_cell_area_box_iter_push_group_width (iter, group->id, group_min_size, group_nat_size);
	  else
	    gtk_cell_area_box_iter_push_group_width_for_height (iter, group->id, for_size,
								group_min_size, group_nat_size);
	}
      else
	{
	  if (for_size < 0)
	    gtk_cell_area_box_iter_push_group_height (iter, group->id, group_min_size, group_nat_size);
	  else
	    gtk_cell_area_box_iter_push_group_height_for_width (iter, group->id, for_size,
								group_min_size, group_nat_size);
	}
    }

  *minimum_size = min_size;
  *natural_size = nat_size;
}

GtkRequestedSize *
get_group_sizes (CellGroup      *group,
		 GtkOrientation  orientation,
		 GtkWidget      *widget,
		 gint           *n_sizes)
{
  GtkRequestedSize *sizes;
  GList            *l;
  gint              i;

  *n_sizes = g_list_length (group->cells);
  sizes    = g_new (GtkRequestedSize, *n_sizes);

  for (l = group->cells, i = 0; l; l = l->next, i++)
    {
      CellInfo *info = l->data;

      sizes[i].data = info;
      
      get_renderer_size (info->renderer,
			 orientation, widget, -1,
			 &sizes[i].minimum_size,
			 &sizes[i].natural_size);
    }

  return sizes;
}

static void
compute_group_size_for_opposing_orientation (GtkCellAreaBox     *box,
					     CellGroup          *group,
					     GtkWidget          *widget, 
					     gint                for_size,
					     gint               *minimum_size, 
					     gint               *natural_size)
{
  GtkCellAreaBoxPrivate *priv = box->priv;

  /* Exception for single cell groups */
  if (!group->cells->next)
    {
      CellInfo *info = group->cells->data;

      get_renderer_size (info->renderer,
			 OPPOSITE_ORIENTATION (priv->orientation),
			 widget, for_size, minimum_size, natural_size);
    }
  else
    {
      GtkRequestedSize *orientation_sizes;
      CellInfo         *info;
      gint              n_sizes, i;
      gint              n_expand_cells = count_expand_cells (group);
      gint              avail_size     = for_size;
      gint              extra_size, extra_extra;
      gint              min_size = 0, nat_size = 0;

      orientation_sizes = get_group_sizes (group, priv->orientation, widget, &n_sizes);

      /* First naturally allocate the cells in the group into the for_size */
      avail_size -= (n_sizes - 1) * priv->spacing;
      for (i = 0; i < n_sizes; i++)
	avail_size -= orientation_sizes[i].minimum_size;

      avail_size = gtk_distribute_natural_allocation (avail_size, n_sizes, orientation_sizes);

      /* Calculate/distribute expand for cells */
      if (n_expand_cells > 0)
	{
	  extra_size  = avail_size / n_expand_cells;
	  extra_extra = avail_size % n_expand_cells;
	}
      else
	extra_size = extra_extra = 0;

      for (i = 0; i < n_sizes; i++)
	{
	  gint cell_min, cell_nat;

	  info = orientation_sizes[i].data;

	  if (info->expand)
	    {
	      orientation_sizes[i].minimum_size += extra_size;
	      if (extra_extra)
		{
		  orientation_sizes[i].minimum_size++;
		  extra_extra--;
		}
	    }

	  get_renderer_size (info->renderer,
			     OPPOSITE_ORIENTATION (priv->orientation),
			     widget, 
			     orientation_sizes[i].minimum_size,
			     &cell_min, &cell_nat);

	  min_size = MAX (min_size, cell_min);
	  nat_size = MAX (nat_size, cell_nat);
	}

      *minimum_size = min_size;
      *natural_size = nat_size;

      g_free (orientation_sizes);
    }
}

static void
compute_size_for_opposing_orientation (GtkCellAreaBox     *box, 
				       GtkCellAreaBoxIter *iter, 
				       GtkWidget          *widget, 
				       gint                for_size,
				       gint               *minimum_size, 
				       gint               *natural_size)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  CellGroup             *group;
  GList                 *group_list;
  GtkRequestedSize      *orientation_sizes;
  gint                   n_groups, n_expand_groups, i;
  gint                   avail_size = for_size;
  gint                   extra_size, extra_extra;
  gint                   min_size = 0, nat_size = 0;

  n_expand_groups = count_expand_groups (box);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    orientation_sizes = gtk_cell_area_box_iter_get_widths (iter, &n_groups);
  else
    orientation_sizes = gtk_cell_area_box_iter_get_heights (iter, &n_groups);

  /* First start by naturally allocating space among groups of cells */
  avail_size -= (n_groups - 1) * priv->spacing;
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

  /* Now we need to naturally allocate sizes for cells in each group
   * and push the height-for-width for each group accordingly while accumulating
   * the overall height-for-width for this row.
   */
  for (group_list = priv->groups; group_list; group_list = group_list->next)
    {
      gint group_min, group_nat;

      group = group_list->data;

      if (group->expand)
	{
	  orientation_sizes[group->id].minimum_size += extra_size;
	  if (extra_extra)
	    {
	      orientation_sizes[group->id].minimum_size++;
	      extra_extra--;
	    }
	}

      /* Now we have the allocation for the group, request it's height-for-width */
      compute_group_size_for_opposing_orientation (box, group, widget,
						   orientation_sizes[group->id].minimum_size,
						   &group_min, &group_nat);

      min_size = MAX (min_size, group_min);
      nat_size = MAX (nat_size, group_nat);

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  gtk_cell_area_box_iter_push_group_height_for_width (iter, group->id, for_size,
							      group_min, group_nat);
	}
      else
	{
	  gtk_cell_area_box_iter_push_group_width_for_height (iter, group->id, for_size,
							      group_min, group_nat);
	}
    }

  *minimum_size = min_size;
  *natural_size = nat_size;

  g_free (orientation_sizes);
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
  gint                   min_width, nat_width;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (iter));

  box_iter = GTK_CELL_AREA_BOX_ITER (iter);

  /* Compute the size of all renderers for current row data, 
   * bumping cell alignments in the iter along the way */
  compute_size (box, GTK_ORIENTATION_HORIZONTAL,
		box_iter, widget, -1, &min_width, &nat_width);

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
  gint                   min_height, nat_height;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_ITER (iter));

  box_iter = GTK_CELL_AREA_BOX_ITER (iter);

  /* Compute the size of all renderers for current row data, 
   * bumping cell alignments in the iter along the way */
  compute_size (box, GTK_ORIENTATION_VERTICAL,
		box_iter, widget, -1, &min_height, &nat_height);

  if (minimum_height)
    *minimum_height = min_height;

  if (natural_height)
    *natural_height = nat_height;
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
      /* Add up vertical requests of height for width and push the overall 
       * cached sizes for alignments */
      compute_size (box, priv->orientation, box_iter, widget, width, &min_height, &nat_height);
    }
  else
    {
      /* Juice: virtually allocate cells into the for_width using the 
       * alignments and then return the overall height for that width, and cache it */
      compute_size_for_opposing_orientation (box, box_iter, widget, width, &min_height, &nat_height);
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
      /* Add up horizontal requests of width for height and push the overall 
       * cached sizes for alignments */
      compute_size (box, priv->orientation, box_iter, widget, height, &min_width, &nat_width);
    }
  else
    {
      /* Juice: horizontally allocate cells into the for_height using the 
       * alignments and then return the overall width for that height, and cache it */
      compute_size_for_opposing_orientation (box, box_iter, widget, height, &min_width, &nat_width);
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
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (cell_layout), renderer, expand, TRUE);
}

static void
gtk_cell_area_box_layout_pack_end (GtkCellLayout      *cell_layout,
				   GtkCellRenderer    *renderer,
				   gboolean            expand)
{
  gtk_cell_area_box_pack_end (GTK_CELL_AREA_BOX (cell_layout), renderer, expand, TRUE);
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

      /* Reconstruct cell groups */
      g_list_foreach (priv->groups, (GFunc)cell_group_free, NULL);
      g_list_free (priv->groups);
      priv->groups = construct_cell_groups (box);
      
      /* Notify that size needs to be requested again */
      flush_iters (box);
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
			       gboolean         expand,
			       gboolean         align)
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

  info = cell_info_new (renderer, GTK_PACK_START, expand, align);

  priv->cells = g_list_append (priv->cells, info);

  /* Reconstruct cell groups */
  g_list_foreach (priv->groups, (GFunc)cell_group_free, NULL);
  g_list_free (priv->groups);
  priv->groups = construct_cell_groups (box);

  /* Notify that size needs to be requested again */
  flush_iters (box);
}

void
gtk_cell_area_box_pack_end (GtkCellAreaBox  *box,
			    GtkCellRenderer *renderer,
			    gboolean         expand, 
			    gboolean         align)
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

  info = cell_info_new (renderer, GTK_PACK_END, expand, align);

  priv->cells = g_list_append (priv->cells, info);

  /* Reconstruct cell groups */
  g_list_foreach (priv->groups, (GFunc)cell_group_free, NULL);
  g_list_free (priv->groups);
  priv->groups = construct_cell_groups (box);

  /* Notify that size needs to be requested again */
  flush_iters (box);
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

      /* Notify that size needs to be requested again */
      flush_iters (box);
    }
}
