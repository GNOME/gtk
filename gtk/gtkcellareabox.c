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
#include "gtkcellareaboxcontext.h"
#include "gtkprivate.h"


/* GObjectClass */
static void      gtk_cell_area_box_finalize                       (GObject              *object);
static void      gtk_cell_area_box_dispose                        (GObject              *object);
static void      gtk_cell_area_box_set_property                   (GObject              *object,
								   guint                 prop_id,
								   const GValue         *value,
								   GParamSpec           *pspec);
static void      gtk_cell_area_box_get_property                   (GObject              *object,
								   guint                 prop_id,
								   GValue               *value,
								   GParamSpec           *pspec);

/* GtkCellAreaClass */
static void      gtk_cell_area_box_add                            (GtkCellArea          *area,
								   GtkCellRenderer      *renderer);
static void      gtk_cell_area_box_remove                         (GtkCellArea          *area,
								   GtkCellRenderer      *renderer);
static void      gtk_cell_area_box_forall                         (GtkCellArea          *area,
								   GtkCellCallback       callback,
								   gpointer              callback_data);
static void      gtk_cell_area_box_get_cell_allocation            (GtkCellArea          *area,
								   GtkCellAreaContext   *context,	
								   GtkWidget            *widget,
								   GtkCellRenderer      *renderer,
								   const GdkRectangle   *cell_area,
								   GdkRectangle         *allocation);
static gint      gtk_cell_area_box_event                          (GtkCellArea          *area,
								   GtkCellAreaContext   *context,
								   GtkWidget            *widget,
								   GdkEvent             *event,
								   const GdkRectangle   *cell_area,
								   GtkCellRendererState  flags);
static void      gtk_cell_area_box_render                         (GtkCellArea          *area,
								   GtkCellAreaContext   *context,
								   GtkWidget            *widget,
								   cairo_t              *cr,
								   const GdkRectangle   *background_area,
								   const GdkRectangle   *cell_area,
								   GtkCellRendererState  flags,
								   gboolean              paint_focus);
static void      gtk_cell_area_box_set_cell_property              (GtkCellArea          *area,
								   GtkCellRenderer      *renderer,
								   guint                 prop_id,
								   const GValue         *value,
								   GParamSpec           *pspec);
static void      gtk_cell_area_box_get_cell_property              (GtkCellArea          *area,
								   GtkCellRenderer      *renderer,
								   guint                 prop_id,
								   GValue               *value,
								   GParamSpec           *pspec);
static GtkCellAreaContext *gtk_cell_area_box_create_context       (GtkCellArea          *area);
static GtkSizeRequestMode  gtk_cell_area_box_get_request_mode     (GtkCellArea          *area);
static void      gtk_cell_area_box_get_preferred_width            (GtkCellArea          *area,
								   GtkCellAreaContext   *context,
								   GtkWidget            *widget,
								   gint                 *minimum_width,
								   gint                 *natural_width);
static void      gtk_cell_area_box_get_preferred_height           (GtkCellArea          *area,
								   GtkCellAreaContext   *context,
								   GtkWidget            *widget,
								   gint                 *minimum_height,
								   gint                 *natural_height);
static void      gtk_cell_area_box_get_preferred_height_for_width (GtkCellArea          *area,
								   GtkCellAreaContext   *context,
								   GtkWidget            *widget,
								   gint                  width,
								   gint                 *minimum_height,
								   gint                 *natural_height);
static void      gtk_cell_area_box_get_preferred_width_for_height (GtkCellArea          *area,
								   GtkCellAreaContext   *context,
								   GtkWidget            *widget,
								   gint                  height,
								   gint                 *minimum_width,
								   gint                 *natural_width);
static gboolean  gtk_cell_area_box_focus                          (GtkCellArea          *area,
								   GtkDirectionType      direction);

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

  guint  id           : 8;
  guint  n_cells      : 8;
  guint  expand_cells : 8;
} CellGroup;

typedef struct {
  GtkCellRenderer *renderer;

  gint             position;
  gint             size;
} AllocatedCell;

static CellInfo      *cell_info_new          (GtkCellRenderer       *renderer, 
					      GtkPackType            pack,
					      gboolean               expand,
					      gboolean               align);
static void           cell_info_free         (CellInfo              *info);
static gint           cell_info_find         (CellInfo              *info,
					      GtkCellRenderer       *renderer);

static AllocatedCell *allocated_cell_new     (GtkCellRenderer       *renderer,
					      gint                   position,
					      gint                   size);
static void           allocated_cell_free    (AllocatedCell         *cell);
static GList         *list_consecutive_cells (GtkCellAreaBox        *box);
static gint           count_expand_groups    (GtkCellAreaBox        *box);
static void           context_weak_notify    (GtkCellAreaBox        *box,
					      GtkCellAreaBoxContext *dead_context);
static void           flush_contexts         (GtkCellAreaBox        *box);
static void           init_context_groups    (GtkCellAreaBox        *box);
static void           init_context_group     (GtkCellAreaBox        *box,
					      GtkCellAreaBoxContext *context);
static GSList        *get_allocated_cells    (GtkCellAreaBox        *box,
					      GtkCellAreaBoxContext *context,
					      GtkWidget             *widget,
					      gint                   orientation_size);


struct _GtkCellAreaBoxPrivate
{
  GtkOrientation  orientation;

  GList          *cells;
  GArray         *groups;

  GSList         *contexts;

  gint            spacing;
};

enum {
  PROP_0,
  PROP_ORIENTATION,
  PROP_SPACING
};

enum {
  CELL_PROP_0,
  CELL_PROP_EXPAND,
  CELL_PROP_ALIGN,
  CELL_PROP_PACK_TYPE
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
  priv->groups      = g_array_new (FALSE, TRUE, sizeof (CellGroup));
  priv->cells       = NULL;
  priv->contexts    = NULL;
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
  area_class->add                 = gtk_cell_area_box_add;
  area_class->remove              = gtk_cell_area_box_remove;
  area_class->forall              = gtk_cell_area_box_forall;
  area_class->get_cell_allocation = gtk_cell_area_box_get_cell_allocation;
  area_class->event               = gtk_cell_area_box_event;
  area_class->render              = gtk_cell_area_box_render;
  area_class->set_cell_property   = gtk_cell_area_box_set_cell_property;
  area_class->get_cell_property   = gtk_cell_area_box_get_cell_property;
  
  area_class->create_context                 = gtk_cell_area_box_create_context;
  area_class->get_request_mode               = gtk_cell_area_box_get_request_mode;
  area_class->get_preferred_width            = gtk_cell_area_box_get_preferred_width;
  area_class->get_preferred_height           = gtk_cell_area_box_get_preferred_height;
  area_class->get_preferred_height_for_width = gtk_cell_area_box_get_preferred_height_for_width;
  area_class->get_preferred_width_for_height = gtk_cell_area_box_get_preferred_width_for_height;

  area_class->focus = gtk_cell_area_box_focus;

  /* Properties */
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

  /* Cell Properties */
  gtk_cell_area_class_install_cell_property (area_class,
					     CELL_PROP_EXPAND,
					     g_param_spec_boolean 
					     ("expand",
					      P_("Expand"),
					      P_("Whether the cell expands"),
					      FALSE,
					      GTK_PARAM_READWRITE));
  
  gtk_cell_area_class_install_cell_property (area_class,
					     CELL_PROP_ALIGN,
					     g_param_spec_boolean
					     ("align",
					      P_("Align"),
					      P_("Whether cell should align with adjacent rows"),
					      TRUE,
					      GTK_PARAM_READWRITE));

  gtk_cell_area_class_install_cell_property (area_class,
					     CELL_PROP_PACK_TYPE,
					     g_param_spec_enum
					     ("pack-type",
					      P_("Pack Type"),
					      P_("A GtkPackType indicating whether the cell is packed with "
						 "reference to the start or end of the cell area"),
					      GTK_TYPE_PACK_TYPE, GTK_PACK_START,
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

static AllocatedCell *
allocated_cell_new (GtkCellRenderer *renderer,
		    gint             position,
		    gint             size)
{
  AllocatedCell *cell = g_slice_new (AllocatedCell);

  cell->renderer = renderer;
  cell->position = position;
  cell->size     = size;

  return cell;
}

static void
allocated_cell_free (AllocatedCell *cell)
{
  g_slice_free (AllocatedCell, cell);
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

static void
cell_groups_clear (GtkCellAreaBox     *box)
{
  GtkCellAreaBoxPrivate *priv  = box->priv;
  gint                   i;

  for (i = 0; i < priv->groups->len; i++)
    {
      CellGroup *group = &g_array_index (priv->groups, CellGroup, i);

      g_list_free (group->cells);
    }

  g_array_set_size (priv->groups, 0);
}

static void
cell_groups_rebuild (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv  = box->priv;
  CellGroup              group = { 0, };
  CellGroup             *group_ptr;
  GList                 *cells, *l;
  guint                  id = 0;

  cell_groups_clear (box);

  if (!priv->cells)
    return;

  cells = list_consecutive_cells (box);

  /* First group is implied */
  g_array_append_val (priv->groups, group);
  group_ptr = &g_array_index (priv->groups, CellGroup, id);

  for (l = cells; l; l = l->next)
    {
      CellInfo *info = l->data;

      /* A new group starts with any aligned cell, the first group is implied */
      if (info->align && l != cells)
	{
	  memset (&group, 0x0, sizeof (CellGroup));
	  group.id = ++id;

	  g_array_append_val (priv->groups, group);
	  group_ptr = &g_array_index (priv->groups, CellGroup, id);
	}

      group_ptr->cells = g_list_prepend (group_ptr->cells, info);
      group_ptr->n_cells++;

      /* A group expands if it contains any expand cells */
      if (info->expand)
	group_ptr->expand_cells++;
    }

  g_list_free (cells);

  for (id = 0; id < priv->groups->len; id++)
    {
      group_ptr = &g_array_index (priv->groups, CellGroup, id);

      group_ptr->cells = g_list_reverse (group_ptr->cells);
    }

  /* Contexts need to be updated with the new grouping information */
  init_context_groups (box);
}

static gint
count_visible_cells (CellGroup *group, 
		     gint      *expand_cells)
{
  GList *l;
  gint   visible_cells = 0;
  gint   n_expand_cells = 0;

  for (l = group->cells; l; l = l->next)
    {
      CellInfo *info = l->data;

      if (gtk_cell_renderer_get_visible (info->renderer))
	{
	  visible_cells++;

	  if (info->expand)
	    n_expand_cells++;
	}
    }

  if (expand_cells)
    *expand_cells = n_expand_cells;

  return visible_cells;
}

static gint
count_expand_groups (GtkCellAreaBox  *box)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  gint                   i;
  gint                   expand_groups = 0;

  for (i = 0; i < priv->groups->len; i++)
    {
      CellGroup *group = &g_array_index (priv->groups, CellGroup, i);

      if (group->expand_cells > 0)
	expand_groups++;
    }

  return expand_groups;
}

static void 
context_weak_notify (GtkCellAreaBox        *box,
		     GtkCellAreaBoxContext *dead_context)
{
  GtkCellAreaBoxPrivate *priv = box->priv;

  priv->contexts = g_slist_remove (priv->contexts, dead_context);
}

static void
init_context_group (GtkCellAreaBox        *box,
		    GtkCellAreaBoxContext *context)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  gint                  *expand_groups, i;

  expand_groups = g_new (gboolean, priv->groups->len);

  for (i = 0; i < priv->groups->len; i++)
    {
      CellGroup *group = &g_array_index (priv->groups, CellGroup, i);

      expand_groups[i] = (group->expand_cells > 0);
    }

  /* This call implies flushing the request info */
  gtk_cell_area_box_init_groups (context, priv->groups->len, expand_groups);
  g_free (expand_groups);
}

static void
init_context_groups (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  GSList                *l;

  /* When the box's groups are reconstructed, contexts need to
   * be reinitialized.
   */
  for (l = priv->contexts; l; l = l->next)
    {
      GtkCellAreaBoxContext *context = l->data;

      init_context_group (box, context);
    }
}

static void
flush_contexts (GtkCellAreaBox *box)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  GSList                *l;

  /* When the box layout changes, contexts need to
   * be flushed and sizes for the box get requested again
   */
  for (l = priv->contexts; l; l = l->next)
    {
      GtkCellAreaContext *context = l->data;

      gtk_cell_area_context_flush (context);
    }
}

/* Returns an allocation for each cell in the orientation of the box,
 * used in ->render()/->event() implementations to get a straight-forward
 * list of allocated cells to operate on.
 */
static GSList *
get_allocated_cells (GtkCellAreaBox        *box,
		     GtkCellAreaBoxContext *context,
		     GtkWidget             *widget,
		     gint                   orientation_size)
{
  GtkCellAreaBoxAllocation *group_allocs;
  GtkCellArea              *area = GTK_CELL_AREA (box);
  GtkCellAreaBoxPrivate    *priv = box->priv;
  GList                    *cell_list;
  GSList                   *allocated_cells = NULL;
  gint                      i, j, n_allocs;
  gboolean                  free_allocs = FALSE;

  group_allocs = gtk_cell_area_box_context_get_orientation_allocs (context, &n_allocs);
  if (!group_allocs)
    {
      group_allocs = gtk_cell_area_box_context_allocate (context, orientation_size, &n_allocs);
      free_allocs  = TRUE;
    }

  for (i = 0; i < n_allocs; i++)
    {
      /* We dont always allocate all groups, sometimes the requested group has only invisible
       * cells for every row, hence the usage of group_allocs[i].group_idx here
       */
      CellGroup *group = &g_array_index (priv->groups, CellGroup, group_allocs[i].group_idx);

      /* Exception for single cell groups */
      if (group->n_cells == 1)
	{
	  CellInfo      *info = group->cells->data;
	  AllocatedCell *cell = 
	    allocated_cell_new (info->renderer, group_allocs[i].position, group_allocs[i].size);

	  allocated_cells = g_slist_prepend (allocated_cells, cell);
	}
      else
	{
	  GtkRequestedSize *sizes;
	  gint              avail_size, position;
	  gint              visible_cells, expand_cells;
	  gint              extra_size, extra_extra;

	  visible_cells = count_visible_cells (group, &expand_cells);

	  /* If this row has no visible cells in this group, just
	   * skip the allocation */
	  if (visible_cells == 0)
	    continue;

	  /* Offset the allocation to the group position and allocate into 
	   * the group's available size */
	  position   = group_allocs[i].position;
	  avail_size = group_allocs[i].size;

	  sizes = g_new (GtkRequestedSize, visible_cells);

	  for (j = 0, cell_list = group->cells; cell_list; cell_list = cell_list->next)
	    {
	      CellInfo *info = cell_list->data;

	      if (!gtk_cell_renderer_get_visible (info->renderer))
		continue;

	      gtk_cell_area_request_renderer (area, info->renderer,
					      priv->orientation,
					      widget, -1,
					      &sizes[j].minimum_size,
					      &sizes[j].natural_size);

	      sizes[j].data = info;
	      avail_size   -= sizes[j].minimum_size;

	      j++;
	    }

	  /* Distribute cells naturally within the group */
	  avail_size -= (visible_cells - 1) * priv->spacing;
	  avail_size = gtk_distribute_natural_allocation (avail_size, visible_cells, sizes);

	  /* Calculate/distribute expand for cells */
	  if (expand_cells > 0)
	    {
	      extra_size  = avail_size / expand_cells;
	      extra_extra = avail_size % expand_cells;
	    }
	  else
	    extra_size = extra_extra = 0;

	  /* Create the allocated cells (loop only over visible cells here) */
	  for (j = 0; j < visible_cells; j++)
	    {
	      CellInfo      *info = sizes[j].data;
	      AllocatedCell *cell;

	      if (info->expand)
		{
		  sizes[j].minimum_size += extra_size;
		  if (extra_extra)
		    {
		      sizes[j].minimum_size++;
		      extra_extra--;
		    }
		}
	      
	      cell = allocated_cell_new (info->renderer, position, sizes[j].minimum_size);

	      allocated_cells = g_slist_prepend (allocated_cells, cell);
	      
	      position += sizes[j].minimum_size;
	      position += priv->spacing;
	    }

	  g_free (sizes);
	}
    }

  if (free_allocs)
    g_free (group_allocs);

  /* Note it might not be important to reverse the list here at all,
   * we have the correct positions, no need to allocate from left to right */
  return g_slist_reverse (allocated_cells);
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

  /* Unref/free the context list */
  for (l = priv->contexts; l; l = l->next)
    g_object_weak_unref (G_OBJECT (l->data), (GWeakNotify)context_weak_notify, box);

  g_slist_free (priv->contexts);
  priv->contexts = NULL;

  /* Free the cell grouping info */
  cell_groups_clear (box);
  g_array_free (priv->groups, TRUE);
  
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
    case PROP_ORIENTATION:
      box->priv->orientation = g_value_get_enum (value);

      /* Notify that size needs to be requested again */
      flush_contexts (box);
      break;
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
    case PROP_ORIENTATION:
      g_value_set_enum (value, box->priv->orientation);
      break;
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
      cell_groups_rebuild (box);
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

static void
gtk_cell_area_box_get_cell_allocation (GtkCellArea          *area,
				       GtkCellAreaContext   *context,	
				       GtkWidget            *widget,
				       GtkCellRenderer      *renderer,
				       const GdkRectangle   *cell_area,
				       GdkRectangle         *allocation)
{
  GtkCellAreaBox        *box      = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv     = box->priv;
  GtkCellAreaBoxContext *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GSList                *allocated_cells, *l;

  *allocation = *cell_area;

  /* Get a list of cells with allocation sizes decided regardless
   * of alignments and pack order etc. */
  allocated_cells = get_allocated_cells (box, box_context, widget, 
					 priv->orientation == GTK_ORIENTATION_HORIZONTAL ?
					 cell_area->width : cell_area->height);

  for (l = allocated_cells; l; l = l->next)
    {
      AllocatedCell *cell = l->data;

      if (cell->renderer == renderer)
	{
	  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      allocation->x     = cell_area->x + cell->position;
	      allocation->width = cell->size;
	    }
	  else
	    {
	      allocation->y      = cell_area->y + cell->position;
	      allocation->height = cell->size;
	    }

	  break;
	}
    }

  g_slist_foreach (allocated_cells, (GFunc)allocated_cell_free, NULL);
  g_slist_free (allocated_cells);
}

enum {
  FOCUS_NONE,
  FOCUS_PREV,
  FOCUS_NEXT
};

static gint
gtk_cell_area_box_event (GtkCellArea          *area,
			 GtkCellAreaContext   *context,
			 GtkWidget            *widget,
			 GdkEvent             *event,
			 const GdkRectangle   *cell_area,
			 GtkCellRendererState  flags)
{
  gint retval;

  /* First let the parent class handle activation of cells via keystrokes */
  retval = 
    GTK_CELL_AREA_CLASS (gtk_cell_area_box_parent_class)->event (area, context, widget,
								 event, cell_area, flags);
  
  if (retval)
    return retval;

  /* Also detect mouse events, for mouse events we need to allocate the renderers
   * and find which renderer needs to be activated.
   */
  if (event->type == GDK_BUTTON_PRESS)
    {
      GdkEventButton *button_event = (GdkEventButton *)event;

      if (button_event->button == 1)
	{
	  GtkCellAreaBox        *box      = GTK_CELL_AREA_BOX (area);
	  GtkCellAreaBoxPrivate *priv     = box->priv;
	  GtkCellAreaBoxContext *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
	  GSList                *allocated_cells, *l;
	  GdkRectangle           cell_background, inner_area;
	  gint                   event_x, event_y;

	  /* We may need some semantics to tell us the offset of the event
	   * window we are handling events for (i.e. GtkTreeView has a bin_window) */
	  event_x = button_event->x;
	  event_y = button_event->y;

	  cell_background = *cell_area;

	  /* Get a list of cells with allocation sizes decided regardless
	   * of alignments and pack order etc. */
	  allocated_cells = get_allocated_cells (box, box_context, widget, 
						 priv->orientation == GTK_ORIENTATION_HORIZONTAL ?
						 cell_area->width : cell_area->height);

	  for (l = allocated_cells; l; l = l->next)
	    {
	      AllocatedCell *cell = l->data;

	      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
		{
		  cell_background.x     = cell_area->x + cell->position;
		  cell_background.width = cell->size;
		}
	      else
		{
		  cell_background.y      = cell_area->y + cell->position;
		  cell_background.height = cell->size;
		}
	      
	      /* Remove margins from the background area to produce the cell area
	       */
	      gtk_cell_area_inner_cell_area (area, widget, &cell_background, &inner_area);
	      
	      if (event_x >= inner_area.x && event_x <= inner_area.x + inner_area.width &&
		  event_y >= inner_area.y && event_y <= inner_area.y + inner_area.height)
		{
		  GtkCellRenderer *event_renderer = NULL;

		  if (gtk_cell_renderer_can_focus (cell->renderer))
		    event_renderer = cell->renderer;
		  else 
		    {
		      GtkCellRenderer *focus_renderer;
		      
		      /* A renderer can have focus siblings but that renderer might not be
		       * focusable for every row... so we go on to check can_focus here. */
		      focus_renderer = gtk_cell_area_get_focus_from_sibling (area, cell->renderer);

		      if (focus_renderer && gtk_cell_renderer_can_focus (focus_renderer))
			event_renderer = focus_renderer;
		    } 

		  if (event_renderer)
		    {
		      if (gtk_cell_area_get_edited_cell (area))
			{
			  /* XXX Was it really canceled in this case ? */
			  gtk_cell_area_stop_editing (area, TRUE);
			  gtk_cell_area_set_focus_cell (area, event_renderer);
			  retval = TRUE;
			}
		      else
			{
			  /* If we are activating via a focus sibling, we need to fix the
			   * cell area */
			  if (event_renderer != cell->renderer)
			    gtk_cell_area_inner_cell_area (area, widget, cell_area, &cell_background);

			  gtk_cell_area_set_focus_cell (area, event_renderer);

			  retval = gtk_cell_area_activate_cell (area, widget, event_renderer,
								event, &cell_background, flags);
			}
		      break;
		    }
		}
	    }
	  g_slist_foreach (allocated_cells, (GFunc)allocated_cell_free, NULL);
	  g_slist_free (allocated_cells);
	}
    }

  return retval;
}

static void
gtk_cell_area_box_render (GtkCellArea          *area,
			  GtkCellAreaContext   *context,
			  GtkWidget            *widget,
			  cairo_t              *cr,
			  const GdkRectangle   *background_area,
			  const GdkRectangle   *cell_area,
			  GtkCellRendererState  flags,
			  gboolean              paint_focus)
{
  GtkCellAreaBox        *box      = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv     = box->priv;
  GtkCellAreaBoxContext *box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  GSList                *allocated_cells, *l;
  GdkRectangle           cell_background, inner_area;
  GtkCellRenderer       *focus_cell = NULL;
  GdkRectangle           focus_rect = { 0, };
  gboolean               first_focus_cell = TRUE;

  if (flags & GTK_CELL_RENDERER_FOCUSED)
    {
      focus_cell = gtk_cell_area_get_focus_cell (area);
      flags &= ~GTK_CELL_RENDERER_FOCUSED;
    }

  cell_background = *cell_area;

  /* Get a list of cells with allocation sizes decided regardless
   * of alignments and pack order etc. */
  allocated_cells = get_allocated_cells (box, box_context, widget, 
					 priv->orientation == GTK_ORIENTATION_HORIZONTAL ?
					 cell_area->width : cell_area->height);

  for (l = allocated_cells; l; l = l->next)
    {
      AllocatedCell       *cell = l->data;
      GtkCellRendererState cell_fields = 0;
      GdkRectangle         render_background;

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  cell_background.x     = cell_area->x + cell->position;
	  cell_background.width = cell->size;
	}
      else
	{
	  cell_background.y      = cell_area->y + cell->position;
	  cell_background.height = cell->size;
	}

      /* Remove margins from the background area to produce the cell area
       */
      gtk_cell_area_inner_cell_area (area, widget, &cell_background, &inner_area);

      /* Add portions of the background_area to the cell_background
       * to create the render_background */
      render_background = cell_background;

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  if (l == allocated_cells)
	    {
	      render_background.width += render_background.x - background_area->x;
	      render_background.x      = background_area->x;
	    }

	  if (l->next == NULL)
	      render_background.width = 
		background_area->width - (render_background.x - background_area->x);

	  render_background.y      = background_area->y;
	  render_background.height = background_area->height;
	}
      else
	{
	  if (l == allocated_cells)
	    {
	      render_background.height += render_background.y - background_area->y;
	      render_background.y       = background_area->y;
	    }

	  if (l->next == NULL)
	      render_background.height = 
		background_area->height - (render_background.y - background_area->y);

	  render_background.x     = background_area->x;
	  render_background.width = background_area->width;
	}

      if (focus_cell && 
	  (cell->renderer == focus_cell || 
	   gtk_cell_area_is_focus_sibling (area, focus_cell, cell->renderer)))
	{
	  cell_fields |= GTK_CELL_RENDERER_FOCUSED;

	  if (paint_focus)
	    {
	      GdkRectangle cell_focus;

	      gtk_cell_area_aligned_cell_area (area, widget, cell->renderer, &inner_area, &cell_focus);

	      /* Accumulate the focus rectangle for all focus siblings */
	      if (first_focus_cell)
		{
		  focus_rect       = cell_focus;
		  first_focus_cell = FALSE;
		}
	      else
		gdk_rectangle_union (&focus_rect, &cell_focus, &focus_rect);
	    }
	}

      /* We have to do some per-cell considerations for the 'flags'
       * for focus handling */
      gtk_cell_renderer_render (cell->renderer, cr, widget,
				&render_background, &inner_area,
				flags | cell_fields);
    }

  if (paint_focus && focus_rect.width != 0 && focus_rect.height != 0)
    {
      GtkStateType renderer_state = 
	flags & GTK_CELL_RENDERER_SELECTED ? GTK_STATE_SELECTED :
	(flags & GTK_CELL_RENDERER_PRELIT ? GTK_STATE_PRELIGHT :
	 (flags & GTK_CELL_RENDERER_INSENSITIVE ? GTK_STATE_INSENSITIVE : GTK_STATE_NORMAL));

      gtk_paint_focus (gtk_widget_get_style (widget), cr, 
		       renderer_state, widget,
		       gtk_cell_area_get_style_detail (area),
		       focus_rect.x,     focus_rect.y,
		       focus_rect.width, focus_rect.height);
    }


  g_slist_foreach (allocated_cells, (GFunc)allocated_cell_free, NULL);
  g_slist_free (allocated_cells);
}

static void
gtk_cell_area_box_set_cell_property (GtkCellArea        *area,
				     GtkCellRenderer    *renderer,
				     guint               prop_id,
				     const GValue       *value,
				     GParamSpec         *pspec)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area); 
  GtkCellAreaBoxPrivate *priv = box->priv;
  GList                 *node;
  CellInfo              *info;
  gboolean               rebuild = FALSE;
  gboolean               val;
  GtkPackType            pack_type;

  node = g_list_find_custom (priv->cells, renderer, 
			     (GCompareFunc)cell_info_find);
  if (!node)
    return;

  info = node->data;

  switch (prop_id)
    {
    case CELL_PROP_EXPAND:
      val = g_value_get_boolean (value);

      if (info->expand != val)
	{
	  info->expand = val;
	  rebuild      = TRUE;
	}
      break;

    case CELL_PROP_ALIGN:
      val = g_value_get_boolean (value);

      if (info->align != val)
	{
	  info->align = val;
	  rebuild     = TRUE;
	}
      break;

    case CELL_PROP_PACK_TYPE:
      pack_type = g_value_get_enum (value);

      if (info->pack != pack_type)
	{
	  info->pack = pack_type;
	  rebuild    = TRUE;
	}
      break;
    default:
      GTK_CELL_AREA_WARN_INVALID_CHILD_PROPERTY_ID (area, prop_id, pspec);
      break;
    }

  /* Groups need to be rebuilt */
  if (rebuild)
    cell_groups_rebuild (box);
}

static void
gtk_cell_area_box_get_cell_property (GtkCellArea        *area,
				     GtkCellRenderer    *renderer,
				     guint               prop_id,
				     GValue             *value,
				     GParamSpec         *pspec)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area); 
  GtkCellAreaBoxPrivate *priv = box->priv;
  GList                 *node;
  CellInfo              *info;

  node = g_list_find_custom (priv->cells, renderer, 
			     (GCompareFunc)cell_info_find);
  if (!node)
    return;

  info = node->data;

  switch (prop_id)
    {
    case CELL_PROP_EXPAND:
      g_value_set_boolean (value, info->expand);
      break;

    case CELL_PROP_ALIGN:
      g_value_set_boolean (value, info->align);
      break;

    case CELL_PROP_PACK_TYPE:
      g_value_set_enum (value, info->pack);
      break;
    default:
      GTK_CELL_AREA_WARN_INVALID_CHILD_PROPERTY_ID (area, prop_id, pspec);
      break;
    }
}


static GtkCellAreaContext *
gtk_cell_area_box_create_context (GtkCellArea *area)
{
  GtkCellAreaBox        *box  = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv = box->priv;
  GtkCellAreaContext    *context =
    (GtkCellAreaContext *)g_object_new (GTK_TYPE_CELL_AREA_BOX_CONTEXT, 
				     "area", area, NULL);

  priv->contexts = g_slist_prepend (priv->contexts, context);

  g_object_weak_ref (G_OBJECT (context), (GWeakNotify)context_weak_notify, box);

  /* Tell the new group about our cell layout */
  init_context_group (box, GTK_CELL_AREA_BOX_CONTEXT (context));

  return context;
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
compute_size (GtkCellAreaBox        *box,
	      GtkOrientation         orientation,
	      GtkCellAreaBoxContext *context,
	      GtkWidget             *widget,
	      gint                   for_size,
	      gint                  *minimum_size,
	      gint                  *natural_size)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  GtkCellArea           *area = GTK_CELL_AREA (box);
  GList                 *list;
  gint                   i;
  gint                   min_size = 0;
  gint                   nat_size = 0;
  
  for (i = 0; i < priv->groups->len; i++)
    {
      CellGroup *group = &g_array_index (priv->groups, CellGroup, i);
      gint       group_min_size = 0;
      gint       group_nat_size = 0;

      for (list = group->cells; list; list = list->next)
	{
	  CellInfo *info = list->data;
	  gint      renderer_min_size, renderer_nat_size;

	  if (!gtk_cell_renderer_get_visible (info->renderer))
	      continue;
	  
	  gtk_cell_area_request_renderer (area, info->renderer, orientation, widget, for_size, 
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
	    gtk_cell_area_box_context_push_group_width (context, group->id, group_min_size, group_nat_size);
	  else
	    gtk_cell_area_box_context_push_group_width_for_height (context, group->id, for_size,
								   group_min_size, group_nat_size);
	}
      else
	{
	  if (for_size < 0)
	    gtk_cell_area_box_context_push_group_height (context, group->id, group_min_size, group_nat_size);
	  else
	    gtk_cell_area_box_context_push_group_height_for_width (context, group->id, for_size,
								   group_min_size, group_nat_size);
	}
    }

  *minimum_size = min_size;
  *natural_size = nat_size;
}

GtkRequestedSize *
get_group_sizes (GtkCellArea    *area,
		 CellGroup      *group,
		 GtkOrientation  orientation,
		 GtkWidget      *widget,
		 gint           *n_sizes)
{
  GtkRequestedSize *sizes;
  GList            *l;
  gint              i;

  *n_sizes = count_visible_cells (group, NULL);
  sizes    = g_new (GtkRequestedSize, *n_sizes);

  for (l = group->cells, i = 0; l; l = l->next)
    {
      CellInfo *info = l->data;

      if (!gtk_cell_renderer_get_visible (info->renderer))
	continue;

      sizes[i].data = info;
      
      gtk_cell_area_request_renderer (area, info->renderer,
				      orientation, widget, -1,
				      &sizes[i].minimum_size,
				      &sizes[i].natural_size);

      i++;
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
  GtkCellArea           *area = GTK_CELL_AREA (box);

  /* Exception for single cell groups */
  if (group->n_cells == 1)
    {
      CellInfo *info = group->cells->data;

      gtk_cell_area_request_renderer (area, info->renderer,
				      OPPOSITE_ORIENTATION (priv->orientation),
				      widget, for_size, minimum_size, natural_size);
    }
  else
    {
      GtkRequestedSize *orientation_sizes;
      CellInfo         *info;
      gint              n_sizes, i;
      gint              avail_size     = for_size;
      gint              extra_size, extra_extra;
      gint              min_size = 0, nat_size = 0;

      orientation_sizes = get_group_sizes (area, group, priv->orientation, widget, &n_sizes);

      /* First naturally allocate the cells in the group into the for_size */
      avail_size -= (n_sizes - 1) * priv->spacing;
      for (i = 0; i < n_sizes; i++)
	avail_size -= orientation_sizes[i].minimum_size;

      avail_size = gtk_distribute_natural_allocation (avail_size, n_sizes, orientation_sizes);

      /* Calculate/distribute expand for cells */
      if (group->expand_cells > 0)
	{
	  extra_size  = avail_size / group->expand_cells;
	  extra_extra = avail_size % group->expand_cells;
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

	  gtk_cell_area_request_renderer (area, info->renderer,
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
compute_size_for_opposing_orientation (GtkCellAreaBox        *box, 
				       GtkCellAreaBoxContext *context, 
				       GtkWidget             *widget, 
				       gint                   for_size,
				       gint                  *minimum_size, 
				       gint                  *natural_size)
{
  GtkCellAreaBoxPrivate *priv = box->priv;
  CellGroup             *group;
  GtkRequestedSize      *orientation_sizes;
  gint                   n_groups, n_expand_groups, i;
  gint                   avail_size = for_size;
  gint                   extra_size, extra_extra;
  gint                   min_size = 0, nat_size = 0;

  n_expand_groups = count_expand_groups (box);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    orientation_sizes = gtk_cell_area_box_context_get_widths (context, &n_groups);
  else
    orientation_sizes = gtk_cell_area_box_context_get_heights (context, &n_groups);

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
  for (i = 0; i < n_groups; i++)
    {
      gint group_min, group_nat;
      gint group_idx = GPOINTER_TO_INT (orientation_sizes[i].data);
      
      group = &g_array_index (priv->groups, CellGroup, group_idx);

      if (group->expand_cells > 0)
	{
	  orientation_sizes[i].minimum_size += extra_size;
	  if (extra_extra)
	    {
	      orientation_sizes[i].minimum_size++;
	      extra_extra--;
	    }
	}

      /* Now we have the allocation for the group, request it's height-for-width */
      compute_group_size_for_opposing_orientation (box, group, widget,
						   orientation_sizes[i].minimum_size,
						   &group_min, &group_nat);

      min_size = MAX (min_size, group_min);
      nat_size = MAX (nat_size, group_nat);

      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	{
	  gtk_cell_area_box_context_push_group_height_for_width (context, group_idx, for_size,
								 group_min, group_nat);
	}
      else
	{
	  gtk_cell_area_box_context_push_group_width_for_height (context, group_idx, for_size,
								 group_min, group_nat);
	}
    }

  *minimum_size = min_size;
  *natural_size = nat_size;

  g_free (orientation_sizes);
}



static void
gtk_cell_area_box_get_preferred_width (GtkCellArea        *area,
				       GtkCellAreaContext *context,
				       GtkWidget          *widget,
				       gint               *minimum_width,
				       gint               *natural_width)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxContext *box_context;
  gint                   min_width, nat_width;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (context));

  box_context = GTK_CELL_AREA_BOX_CONTEXT (context);

  /* Compute the size of all renderers for current row data, 
   * bumping cell alignments in the context along the way */
  compute_size (box, GTK_ORIENTATION_HORIZONTAL,
		box_context, widget, -1, &min_width, &nat_width);

  if (minimum_width)
    *minimum_width = min_width;

  if (natural_width)
    *natural_width = nat_width;
}

static void
gtk_cell_area_box_get_preferred_height (GtkCellArea        *area,
					GtkCellAreaContext *context,
					GtkWidget          *widget,
					gint               *minimum_height,
					gint               *natural_height)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxContext *box_context;
  gint                   min_height, nat_height;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (context));

  box_context = GTK_CELL_AREA_BOX_CONTEXT (context);

  /* Compute the size of all renderers for current row data, 
   * bumping cell alignments in the context along the way */
  compute_size (box, GTK_ORIENTATION_VERTICAL,
		box_context, widget, -1, &min_height, &nat_height);

  if (minimum_height)
    *minimum_height = min_height;

  if (natural_height)
    *natural_height = nat_height;
}

static void
gtk_cell_area_box_get_preferred_height_for_width (GtkCellArea        *area,
						  GtkCellAreaContext *context,
						  GtkWidget          *widget,
						  gint                width,
						  gint               *minimum_height,
						  gint               *natural_height)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxContext *box_context;
  GtkCellAreaBoxPrivate *priv;
  gint                   min_height, nat_height;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (context));

  box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  priv        = box->priv;

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    {
      /* Add up vertical requests of height for width and push the overall 
       * cached sizes for alignments */
      compute_size (box, priv->orientation, box_context, widget, width, &min_height, &nat_height);
    }
  else
    {
      /* Juice: virtually allocate cells into the for_width using the 
       * alignments and then return the overall height for that width, and cache it */
      compute_size_for_opposing_orientation (box, box_context, widget, width, &min_height, &nat_height);
    }

  if (minimum_height)
    *minimum_height = min_height;

  if (natural_height)
    *natural_height = nat_height;
}

static void
gtk_cell_area_box_get_preferred_width_for_height (GtkCellArea        *area,
						  GtkCellAreaContext *context,
						  GtkWidget          *widget,
						  gint                height,
						  gint               *minimum_width,
						  gint               *natural_width)
{
  GtkCellAreaBox        *box = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxContext *box_context;
  GtkCellAreaBoxPrivate *priv;
  gint                   min_width, nat_width;

  g_return_if_fail (GTK_IS_CELL_AREA_BOX_CONTEXT (context));

  box_context = GTK_CELL_AREA_BOX_CONTEXT (context);
  priv        = box->priv;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* Add up horizontal requests of width for height and push the overall 
       * cached sizes for alignments */
      compute_size (box, priv->orientation, box_context, widget, height, &min_width, &nat_width);
    }
  else
    {
      /* Juice: horizontally allocate cells into the for_height using the 
       * alignments and then return the overall width for that height, and cache it */
      compute_size_for_opposing_orientation (box, box_context, widget, height, &min_width, &nat_width);
    }

  if (minimum_width)
    *minimum_width = min_width;

  if (natural_width)
    *natural_width = nat_width;
}

static gboolean
gtk_cell_area_box_focus (GtkCellArea      *area,
			 GtkDirectionType  direction)
{
  GtkCellAreaBox        *box   = GTK_CELL_AREA_BOX (area);
  GtkCellAreaBoxPrivate *priv  = box->priv;
  gint                   cycle = FOCUS_NONE;
  gboolean               cycled_focus = FALSE;
  GtkCellRenderer       *focus_cell;

  focus_cell = gtk_cell_area_get_focus_cell (area);

  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
      cycle = FOCUS_NEXT;
      break;
    case GTK_DIR_TAB_BACKWARD:
      cycle = FOCUS_PREV;
      break;
    case GTK_DIR_UP: 
      if (priv->orientation == GTK_ORIENTATION_VERTICAL || !focus_cell)
	cycle = FOCUS_PREV;
      break;
    case GTK_DIR_DOWN:
      if (priv->orientation == GTK_ORIENTATION_VERTICAL || !focus_cell)
	cycle = FOCUS_NEXT;
      break;
    case GTK_DIR_LEFT:
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL || !focus_cell)
	cycle = FOCUS_PREV;
      break;
    case GTK_DIR_RIGHT:
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL || !focus_cell)
	cycle = FOCUS_NEXT;
      break;
    default:
      break;
    }

  if (cycle != FOCUS_NONE)
    {
      gboolean  found_cell = FALSE;
      GList    *list;
      gint      i;

      /* If there is no focused cell, focus on the first (or last) one in the list */
      if (!focus_cell)
	found_cell = TRUE;

      for (i = (cycle == FOCUS_NEXT) ? 0 : priv->groups->len -1; 
	   cycled_focus == FALSE && i >= 0 && i < priv->groups->len;
	   i = (cycle == FOCUS_NEXT) ? i + 1 : i - 1)
	{
	  CellGroup *group = &g_array_index (priv->groups, CellGroup, i);
	  
	  for (list = (cycle == FOCUS_NEXT) ? g_list_first (group->cells) : g_list_last (group->cells); 
	       list; list = (cycle == FOCUS_NEXT) ? list->next : list->prev)
	    {
	      CellInfo *info = list->data;

	      if (info->renderer == focus_cell)
		found_cell = TRUE;
	      else if (found_cell)
		{
		  if (gtk_cell_renderer_can_focus (info->renderer))
		    {
		      gtk_cell_area_set_focus_cell (area, info->renderer);

		      cycled_focus = TRUE;
		      break;
		    }
		}
	    }
	}
    }

  if (!cycled_focus)
    gtk_cell_area_set_focus_cell (area, NULL);

  return cycled_focus;
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

      cell_groups_rebuild (box);
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

  cell_groups_rebuild (box);
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

  cell_groups_rebuild (box);
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
      flush_contexts (box);
    }
}
